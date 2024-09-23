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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.content.R;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.media.mojom.SpeechRecognitionErrorCode;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/** Implementation of {@link SpeechRecognition}. */
@JNINamespace("content")
public class SpeechRecognitionImpl {
    private static final String TAG = "SpeechRecog";

    // Constants describing the speech recognition provider we depend on.
    private static final String AGSA_PACKAGE_NAME = "com.google.android.googlequicksearchbox";
    private static final int AGSA_MIN_VERSION = 300207030;
    private static final String SSBG_PACKAGE_NAME = "com.google.android.tts";

    // We track the recognition state to remember what events we need to send when recognition is
    // being aborted. Once Android's recognizer is cancelled, its listener won't yield any more
    // events, but we still need to call OnSoundEnd and OnAudioEnd if corresponding On*Start were
    // called before.
    private static final int STATE_IDLE = 0;
    private static final int STATE_AWAITING_SPEECH = 1;
    private static final int STATE_CAPTURING_SPEECH = 2;
    private int mState;

    // The speech recognition provider (if any) matching PROVIDER_PACKAGE_NAME and
    // PROVIDER_MIN_VERSION as selected by initialize().
    private static ComponentName sRecognitionProvider;

    private final Intent mIntent;
    private final RecognitionListener mListener;
    private SpeechRecognizer mRecognizer;

    // Native pointer to C++ SpeechRecognizerImplAndroid.
    private long mNativeSpeechRecognizerImplAndroid;

    // Remember if we are using continuous recognition.
    private boolean mContinuous;

    // Internal class to handle events from Android's SpeechRecognizer and route them to native.
    class Listener implements RecognitionListener {

        @Override
        public void onBeginningOfSpeech() {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;

            mState = STATE_CAPTURING_SPEECH;
            SpeechRecognitionImplJni.get()
                    .onSoundStart(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
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
                SpeechRecognitionImplJni.get()
                        .onSoundEnd(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
                // Since Android doesn't have a dedicated event for when audio capture is finished,
                // we fire it after speech has ended.
                SpeechRecognitionImplJni.get()
                        .onAudioEnd(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
                mState = STATE_IDLE;
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
            handleResults(bundle, true);
        }

        @Override
        public void onReadyForSpeech(Bundle bundle) {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;

            mState = STATE_AWAITING_SPEECH;
            SpeechRecognitionImplJni.get()
                    .onAudioStart(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
        }

        @Override
        public void onResults(Bundle bundle) {
            handleResults(bundle, false);
            // We assume that onResults is called only once, at the end of a session, thus we
            // terminate. If one day the recognition provider changes dictation mode behavior to
            // call onResults several times, we should terminate only if (!mContinuous).
            terminate(SpeechRecognitionErrorCode.NONE);
        }

        @Override
        public void onRmsChanged(float rms) {}

        private void handleResults(Bundle bundle, boolean provisional) {
            if (mNativeSpeechRecognizerImplAndroid == 0) return;

            if (mContinuous && provisional) {
                // In continuous mode, Android's recognizer sends final results as provisional.
                provisional = false;
            }

            ArrayList<String> list =
                    bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            String[] results = list.toArray(new String[list.size()]);

            float[] scores = bundle.getFloatArray(SpeechRecognizer.CONFIDENCE_SCORES);

            SpeechRecognitionImplJni.get()
                    .onRecognitionResults(
                            mNativeSpeechRecognizerImplAndroid,
                            SpeechRecognitionImpl.this,
                            results,
                            scores,
                            provisional);
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
    private static ComponentName createRecognitionProvider() {
        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                || BuildInfo.getInstance().isAutomotive) {
            return getComponent(SSBG_PACKAGE_NAME, -1);
        } else {
            return getComponent(AGSA_PACKAGE_NAME, AGSA_MIN_VERSION);
        }
    }

    @SuppressLint("WrongConstant")
    private static ComponentName getComponent(String packageName, int packageMinVersion) {
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

    private SpeechRecognitionImpl(long nativeSpeechRecognizerImplAndroid) {
        mContinuous = false;
        mNativeSpeechRecognizerImplAndroid = nativeSpeechRecognizerImplAndroid;
        mListener = new Listener();
        mIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);

        if (sRecognitionProvider != null) {
            mRecognizer =
                    SpeechRecognizer.createSpeechRecognizer(
                            ContextUtils.getApplicationContext(), sRecognitionProvider);
        } else {
            // It is possible to force-enable the speech recognition web platform feature (using a
            // command-line flag) even if initialize() failed to find the PROVIDER_PACKAGE_NAME
            // provider, in which case the first available speech recognition provider is used.
            // Caveat: Continuous mode may not work as expected with a different provider.
            mRecognizer =
                    SpeechRecognizer.createSpeechRecognizer(ContextUtils.getApplicationContext());
        }

        mRecognizer.setRecognitionListener(mListener);
    }

    // This function destroys everything when recognition is done, taking care to properly tear
    // down by calling On{Sound,Audio}End if corresponding On{Audio,Sound}Start were called.
    private void terminate(int error) {
        if (mNativeSpeechRecognizerImplAndroid == 0) return;

        if (mState != STATE_IDLE) {
            if (mState == STATE_CAPTURING_SPEECH) {
                SpeechRecognitionImplJni.get()
                        .onSoundEnd(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
            }
            SpeechRecognitionImplJni.get()
                    .onAudioEnd(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
            mState = STATE_IDLE;
        }

        if (error != SpeechRecognitionErrorCode.NONE) {
            SpeechRecognitionImplJni.get()
                    .onRecognitionError(
                            mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this, error);
        }

        try {
            mRecognizer.destroy();
        } catch (IllegalArgumentException e) {
            // Intentionally swallow exception. This incorrectly throws exception on some samsung
            // devices, causing crashes.
            Log.w(TAG, "Destroy threw exception " + mRecognizer, e);
        }
        mRecognizer = null;
        SpeechRecognitionImplJni.get()
                .onRecognitionEnd(mNativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl.this);
        mNativeSpeechRecognizerImplAndroid = 0;
    }

    @CalledByNative
    private static SpeechRecognitionImpl createSpeechRecognition(
            long nativeSpeechRecognizerImplAndroid) {
        return new SpeechRecognitionImpl(nativeSpeechRecognizerImplAndroid);
    }

    @CalledByNative
    private void startRecognition(String language, boolean continuous, boolean interimResults) {
        if (mRecognizer == null) return;

        mContinuous = continuous;
        mIntent.putExtra("android.speech.extra.DICTATION_MODE", continuous);
        mIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE, language);
        mIntent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, interimResults);
        try {
            mRecognizer.startListening(mIntent);
        } catch (SecurityException e) {
            Context context = ContextUtils.getApplicationContext();
            String msg =
                    context.getResources().getString(R.string.speech_recognition_service_not_found);
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
        }
    }

    @CalledByNative
    private void abortRecognition() {
        if (mRecognizer == null) return;

        mRecognizer.cancel();
        terminate(SpeechRecognitionErrorCode.ABORTED);
    }

    @CalledByNative
    private void stopRecognition() {
        if (mRecognizer == null) return;

        mContinuous = false;
        mRecognizer.stopListening();
    }

    @NativeMethods
    interface Natives {
        // Native JNI calls to content/browser/speech/speech_recognizer_impl_android.cc
        void onAudioStart(long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller);

        void onSoundStart(long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller);

        void onSoundEnd(long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller);

        void onAudioEnd(long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller);

        void onRecognitionResults(
                long nativeSpeechRecognizerImplAndroid,
                SpeechRecognitionImpl caller,
                String[] results,
                float[] scores,
                boolean provisional);

        void onRecognitionError(
                long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller, int error);

        void onRecognitionEnd(long nativeSpeechRecognizerImplAndroid, SpeechRecognitionImpl caller);
    }
}
