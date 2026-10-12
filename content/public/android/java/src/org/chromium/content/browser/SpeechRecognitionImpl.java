// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognitionService;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.media.mojom.SpeechRecognitionErrorCode;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Implementation of {@link SpeechRecognition}. */
@JNINamespace("content")
@NullMarked
public class SpeechRecognitionImpl {
    private static final String TAG = "SpeechRecog";

    // Constants describing the speech recognition provider we depend on.
    private static final String AGSA_PACKAGE_NAME = "com.google.android.googlequicksearchbox";
    private static final int AGSA_MIN_VERSION = 300207030;
    private static final String SSBG_PACKAGE_NAME = "com.google.android.tts";

    // Undocumented extras defined internally by the Google recognition service
    // (com.google.android.tts and AGSA) in addition to the public RecognizerIntent extras:
    // dictation mode for legacy recognizers, low-confidence text for the words still being
    // recognized in a partial result, and a flag marking a partial result as a finalized segment on
    // devices without segmented sessions.
    private static final String EXTRA_DICTATION_MODE = "android.speech.extra.DICTATION_MODE";
    private static final String EXTRA_UNSTABLE_TEXT = "android.speech.extra.UNSTABLE_TEXT";
    private static final String EXTRA_FINAL_RESULT = "final_result";

    // Maximum value accepted by the Google recognition service for
    // EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS (5 minutes). In continuous mode, this keeps the
    // session open across pauses between utterances while each utterance is finalized and delivered
    // via onSegmentResults() (or onPartialResults() with EXTRA_FINAL_RESULT before Android T).
    private static final int CONTINUOUS_SPEECH_INPUT_MINIMUM_LENGTH_MS = 5 * 60 * 1000;

    // We track the recognition state to remember what events we need to send when recognition is
    // being aborted or terminated, and to prevent duplicate start/end events across segments in
    // continuous mode. Once Android's recognizer is cancelled, its listener won't yield any more
    // events, but we still need to call OnSoundEnd and OnAudioEnd if corresponding On*Start were
    // called before.
    private static final int STATE_IDLE = 0;
    private static final int STATE_AWAITING_SPEECH = 1;
    private static final int STATE_CAPTURING_SPEECH = 2;
    private static final int STATE_AWAITING_FINAL_RESULT = 3;
    private int mState;

    // The speech recognition provider (if any) matching PROVIDER_PACKAGE_NAME and
    // PROVIDER_MIN_VERSION as selected by initialize().
    private static @Nullable ComponentName sRecognitionProvider;

    private final Intent mIntent;
    private final RecognitionListener mListener;
    private @Nullable SpeechRecognizer mRecognizer;

    // Native pointer to C++ SpeechRecognizerImplAndroid.
    private long mNativeSpeechRecognizerImplAndroid;

    // Remember if we are using continuous recognition.
    private boolean mContinuous;

    // Remember if interim results are requested.
    private boolean mInterimResults;

    // Internal class to handle events from Android's SpeechRecognizer and route them to native.
    class Listener implements RecognitionListener {

        @Override
        public void onBeginningOfSpeech() {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;
            if (mState != STATE_AWAITING_SPEECH) return;

            mState = STATE_CAPTURING_SPEECH;
            SpeechRecognitionImplJni.get().onSoundStart(mNativeSpeechRecognizerImplAndroid);
        }

        @Override
        public void onBufferReceived(byte[] buffer) {}

        @Override
        public void onEndOfSpeech() {
            // Ignore onEndOfSpeech in continuous mode to let terminate() take care of ending
            // events. The Android API documentation is vague as to when onEndOfSpeech is called in
            // continuous mode, whereas the Web Speech API defines a stronger semantic on the
            // equivalent (onsoundend) event. Thus, the only way to provide a valid onsoundend
            // event is to trigger it when the last result is received or the session is aborted.
            if (!mContinuous) {
                if (mNativeSpeechRecognizerImplAndroid == 0) return;
                if (mState == STATE_AWAITING_SPEECH || mState == STATE_CAPTURING_SPEECH) {
                    if (mState == STATE_CAPTURING_SPEECH) {
                        SpeechRecognitionImplJni.get()
                                .onSoundEnd(mNativeSpeechRecognizerImplAndroid);
                    }
                    // Since Android doesn't have a dedicated event for when audio capture is
                    // finished, we fire it after speech has ended.
                    SpeechRecognitionImplJni.get().onAudioEnd(mNativeSpeechRecognizerImplAndroid);
                    mState = STATE_AWAITING_FINAL_RESULT;
                }
            }
        }

        @Override
        public void onError(int error) {
            int code = SpeechRecognitionErrorCode.NONE;

            // Translate Android SpeechRecognizer errors to Web Speech API errors.
            switch (error) {
                case SpeechRecognizer.ERROR_AUDIO:
                    code = SpeechRecognitionErrorCode.AUDIO_CAPTURE;
                    break;
                case SpeechRecognizer.ERROR_CLIENT:
                    code = SpeechRecognitionErrorCode.ABORTED;
                    break;
                case SpeechRecognizer.ERROR_RECOGNIZER_BUSY:
                case SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS:
                    code = SpeechRecognitionErrorCode.NOT_ALLOWED;
                    break;
                case SpeechRecognizer.ERROR_NETWORK_TIMEOUT:
                case SpeechRecognizer.ERROR_NETWORK:
                case SpeechRecognizer.ERROR_SERVER:
                    code = SpeechRecognitionErrorCode.NETWORK;
                    break;
                case SpeechRecognizer.ERROR_NO_MATCH:
                    code = SpeechRecognitionErrorCode.NO_MATCH;
                    break;
                case SpeechRecognizer.ERROR_SPEECH_TIMEOUT:
                    code = SpeechRecognitionErrorCode.NO_SPEECH;
                    break;
                default:
                    assert false;
                    return;
            }

            terminate(code);
        }

        @Override
        public void onEvent(int event, Bundle bundle) {}

        @Override
        public void onPartialResults(Bundle bundle) {
            // When a segmented session was not requested (or is not supported, before Android T),
            // the recognizer delivers the finalized utterances of a continuous session through
            // onPartialResults() flagged as final, instead of through onSegmentResults(), and
            // ends the session with an empty onResults().
            boolean provisional = !bundle.getBoolean(EXTRA_FINAL_RESULT, false);
            handleResults(bundle, provisional);
        }

        @Override
        public void onReadyForSpeech(Bundle bundle) {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;
            if (mState != STATE_IDLE) return;

            mState = STATE_AWAITING_SPEECH;
            SpeechRecognitionImplJni.get().onAudioStart(mNativeSpeechRecognizerImplAndroid);
        }

        @Override
        public void onResults(Bundle bundle) {
            handleResults(bundle, /* provisional= */ false);
            // We assume that onResults is called only once, at the end of a session, thus we
            // terminate. If one day the recognition provider changes dictation mode behavior to
            // call onResults several times, we should terminate only if (!mContinuous).
            terminate(SpeechRecognitionErrorCode.NONE);
        }

        @Override
        public void onSegmentResults(Bundle bundle) {
            handleResults(bundle, /* provisional= */ false);
        }

        @Override
        public void onEndOfSegmentedSession() {
            terminate(SpeechRecognitionErrorCode.NONE);
        }

        @Override
        public void onRmsChanged(float rms) {}

        private void handleResults(Bundle bundle, boolean provisional) {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;
            if (provisional && !mInterimResults) return;

            // Not every bundle carries a transcript: an empty bundle ends a continuous session
            // without segmented session support, and some bundles only carry metadata such as
            // the detected language.
            ArrayList<String> list =
                    bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            if (list == null || list.isEmpty()) return;

            String[] results = new String[list.size()];
            for (int i = 0; i < list.size(); i++) {
                String hypothesis = list.get(i);
                results[i] = hypothesis != null ? hypothesis : "";
            }

            if (provisional) {
                // An interim result is a single hypothesis split into stable text and
                // low-confidence text for the words still being recognized, formatted so that
                // concatenating them gives the complete hypothesis so far, which is what the Web
                // Speech API's interim transcript is.
                ArrayList<String> unstable = bundle.getStringArrayList(EXTRA_UNSTABLE_TEXT);
                if (unstable != null && !unstable.isEmpty() && unstable.get(0) != null) {
                    results[0] += unstable.get(0);
                }
            }
            if (TextUtils.isEmpty(results[0])) return;

            float[] scores = bundle.getFloatArray(SpeechRecognizer.CONFIDENCE_SCORES);
            if (!hasConfidenceScores(scores, results.length)) {
                // The recognizer did not estimate its confidence (the on-device recognizer
                // reports 0 for every hypothesis in that case). Report full confidence, like
                // Chrome's own on-device recognition engines do, rather than no confidence. The
                // native side requires one score per result.
                scores = new float[results.length];
                Arrays.fill(scores, 1.0f);
            }

            SpeechRecognitionImplJni.get()
                    .onRecognitionResults(
                            mNativeSpeechRecognizerImplAndroid, results, scores, provisional);
        }
    }

    /**
     * This method must be called before any instance of SpeechRecognition can be created. It will
     * query Android's package manager to find a suitable speech recognition provider that supports
     * continuous recognition.
     */
    // TODO(crbug.com/40479664): Fix this properly.
    public static boolean initialize() {
        Context context = ContextUtils.getApplicationContext();
        if (!SpeechRecognizer.isRecognitionAvailable(context)) return false;

        sRecognitionProvider = createRecognitionProvider();
        if (sRecognitionProvider == null) return false;
        return true;
    }

    /** Returns null if there is no Google LLC provided RecognitionService available on device. */
    private static @Nullable ComponentName createRecognitionProvider() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S || DeviceInfo.isAutomotive()) {
            return getComponent(SSBG_PACKAGE_NAME, -1);
        } else {
            return getComponent(AGSA_PACKAGE_NAME, AGSA_MIN_VERSION);
        }
    }

    @SuppressLint("WrongConstant")
    private static @Nullable ComponentName getComponent(String packageName, int packageMinVersion) {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        Intent intent = new Intent(RecognitionService.SERVICE_INTERFACE);
        final List<ResolveInfo> list = pm.queryIntentServices(intent, PackageManager.GET_SERVICES);

        for (ResolveInfo resolve : list) {
            ServiceInfo service = resolve.serviceInfo;

            if (!service.packageName.equals(packageName)) continue;

            // No need to verify package version if packageMinVersion != -1.
            if (packageMinVersion != -1) {
                if (PackageUtils.getPackageVersion(service.packageName) < packageMinVersion) {
                    continue;
                }
            }

            return new ComponentName(service.packageName, service.name);
        }

        return null;
    }

    /**
     * Returns whether the recognizer provided usable confidence scores for the results. On-device
     * recognizers populate the array with 0.0f for all hypotheses, while other recognizers may use
     * negative values when confidence is unavailable.
     */
    private static boolean hasConfidenceScores(float @Nullable [] scores, int resultCount) {
        if (scores == null || scores.length != resultCount) return false;
        boolean hasPositiveScore = false;
        for (float score : scores) {
            if (!(score >= 0.0f && score <= 1.0f)) return false;
            if (score > 0.0f) hasPositiveScore = true;
        }
        return hasPositiveScore;
    }

    @VisibleForTesting
    SpeechRecognitionImpl(long nativeSpeechRecognizerImplAndroid, SpeechRecognizer recognizer) {
        mContinuous = false;
        mInterimResults = false;
        mNativeSpeechRecognizerImplAndroid = nativeSpeechRecognizerImplAndroid;
        mListener = new Listener();
        mIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        mRecognizer = recognizer;
        mRecognizer.setRecognitionListener(mListener);
    }

    // This function destroys everything when recognition is done, taking care to properly tear
    // down by calling On{Sound,Audio}End if corresponding On{Audio,Sound}Start were called.
    private void terminate(int error) {
        if (mNativeSpeechRecognizerImplAndroid == 0) return;

        if (mState == STATE_AWAITING_SPEECH || mState == STATE_CAPTURING_SPEECH) {
            if (mState == STATE_CAPTURING_SPEECH) {
                SpeechRecognitionImplJni.get().onSoundEnd(mNativeSpeechRecognizerImplAndroid);
            }
            SpeechRecognitionImplJni.get().onAudioEnd(mNativeSpeechRecognizerImplAndroid);
        }
        mState = STATE_IDLE;

        if (error != SpeechRecognitionErrorCode.NONE) {
            SpeechRecognitionImplJni.get()
                    .onRecognitionError(mNativeSpeechRecognizerImplAndroid, error);
        }

        try {
            if (mRecognizer != null) {
                mRecognizer.destroy();
            }
        } catch (IllegalArgumentException e) {
            // Intentionally swallow exception. This incorrectly throws exception on some samsung
            // devices, causing crashes.
            Log.w(TAG, "Destroy threw exception " + mRecognizer, e);
        }
        mRecognizer = null;
        SpeechRecognitionImplJni.get().onRecognitionEnd(mNativeSpeechRecognizerImplAndroid);
        mNativeSpeechRecognizerImplAndroid = 0;
    }

    @CalledByNative
    @VisibleForTesting
    static SpeechRecognitionImpl createSpeechRecognition(long nativeSpeechRecognizerImplAndroid) {
        SpeechRecognizer recognizer;
        if (sRecognitionProvider != null) {
            recognizer =
                    SpeechRecognizer.createSpeechRecognizer(
                            ContextUtils.getApplicationContext(), sRecognitionProvider);
        } else {
            // It is possible to force-enable the speech recognition web platform feature (using a
            // command-line flag) even if initialize() failed to find the PROVIDER_PACKAGE_NAME
            // provider, in which case the first available speech recognition provider is used.
            // Caveat: Continuous mode may not work as expected with a different provider.
            recognizer =
                    SpeechRecognizer.createSpeechRecognizer(ContextUtils.getApplicationContext());
        }
        return new SpeechRecognitionImpl(nativeSpeechRecognizerImplAndroid, recognizer);
    }

    @CalledByNative
    @VisibleForTesting
    void startRecognition(String language, boolean continuous, boolean interimResults) {
        if (mRecognizer == null) return;

        mContinuous = continuous;
        mInterimResults = interimResults;
        // Older Google recognizers used this extra for continuous recognition; current ones
        // ignore it.
        mIntent.putExtra(EXTRA_DICTATION_MODE, continuous);
        mIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE, language);
        boolean requestPartials =
                interimResults
                        || (continuous && Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU);
        mIntent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, requestPartials);
        if (continuous) {
            // Keep the session going across utterances while finalizing each utterance via
            // onSegmentResults() (or onPartialResults() flagged with EXTRA_FINAL_RESULT before
            // Android T).
            mIntent.putExtra(
                    RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS,
                    CONTINUOUS_SPEECH_INPUT_MINIMUM_LENGTH_MS);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                mIntent.putExtra(
                        RecognizerIntent.EXTRA_SEGMENTED_SESSION,
                        RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS);
            }
        }
        try {
            mRecognizer.startListening(mIntent);
        } catch (SecurityException e) {
            Context context = ContextUtils.getApplicationContext();
            String msg = context.getString(R.string.speech_recognition_service_not_found);
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
        }
    }

    @CalledByNative
    @VisibleForTesting
    void abortRecognition() {
        if (mRecognizer == null) return;

        mRecognizer.cancel();
        terminate(SpeechRecognitionErrorCode.ABORTED);
    }

    @CalledByNative
    @VisibleForTesting
    void stopRecognition() {
        if (mRecognizer == null) return;

        mContinuous = false;
        mRecognizer.stopListening();
    }

    Intent getIntentForTesting() {
        return mIntent;
    }

    RecognitionListener getListenerForTesting() {
        return mListener;
    }

    @NativeMethods
    interface Natives {
        // Native JNI calls to content/browser/speech/speech_recognizer_impl_android.cc
        void onAudioStart(long nativeSpeechRecognizerImplAndroid);

        void onSoundStart(long nativeSpeechRecognizerImplAndroid);

        void onSoundEnd(long nativeSpeechRecognizerImplAndroid);

        void onAudioEnd(long nativeSpeechRecognizerImplAndroid);

        void onRecognitionResults(
                long nativeSpeechRecognizerImplAndroid,
                String[] results,
                float @Nullable [] scores,
                boolean provisional);

        void onRecognitionError(long nativeSpeechRecognizerImplAndroid, int error);

        void onRecognitionEnd(long nativeSpeechRecognizerImplAndroid);
    }
}
