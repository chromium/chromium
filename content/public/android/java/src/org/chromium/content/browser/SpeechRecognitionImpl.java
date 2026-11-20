// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.speech.RecognitionListener;
import android.speech.RecognitionService;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.SpeechRecognition;
import org.chromium.media.mojom.SpeechRecognitionErrorCode;
import org.chromium.ui.widget.Toast;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

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

    // In-process microphone capture (gated on Android T+ and the
    // AndroidSpeechRecognitionInProcessAudio feature flag in startRecognition()):
    // By default, Android's SpeechRecognizer opens the mic inside the separate recognition service
    // process (com.google.android.tts), which Android's AudioPolicyService classifies as background
    // and silences whenever another foreground mic stream is active. Capturing 16 kHz mono 16-bit
    // PCM inside Chrome's foreground process via AudioRecord and piping it to EXTRA_AUDIO_SOURCE
    // gives speech recognition foreground mic priority and matches SpeechRecognizer's native format
    // without resampling.
    private static final int AUDIO_SAMPLE_RATE_HZ = 16000;
    private static final int AUDIO_CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO;
    private static final int AUDIO_CHANNEL_COUNT = 1;
    private static final int AUDIO_ENCODING = AudioFormat.ENCODING_PCM_16BIT;
    // 20 ms per chunk at 16 kHz * 1 channel * 2 bytes/sample = 640 bytes. Short reads bound how
    // long the audio thread keeps capturing after the session closes.
    private static final int AUDIO_READ_BUFFER_BYTES = 640;

    // When given RecognizerIntent.EXTRA_AUDIO_SOURCE, the Google recognition service disables its
    // default 5 s silence timeout and, unless a segmented session is requested, delivers finalized
    // utterances via onPartialResults() rather than onSegmentResults(). Single-utterance sessions
    // restore the timeout through a segmented session keyed off
    // EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS.
    private static final int SINGLE_UTTERANCE_COMPLETE_SILENCE_LENGTH_MS = 5000;

    /**
     * Seam over {@link AudioRecord} so Robolectric unit tests can inject fake 16-bit mono 16 kHz
     * PCM bytes without real microphone hardware. Only used on the audio thread.
     */
    interface PcmAudioInput {
        /**
         * Reads up to {@code length} PCM bytes into {@code buffer} starting at {@code offset}.
         * Returns the number of bytes read, 0 on end-of-stream, or a negative error code.
         */
        int read(byte[] buffer, int offset, int length);

        /** Stops and releases the underlying audio capture resource. */
        void close();
    }

    /**
     * Factory for opening a started {@link PcmAudioInput} for an in-process audio session. Called
     * on the audio thread, since creating and starting an {@link AudioRecord} makes blocking Binder
     * calls.
     */
    interface PcmAudioInputFactory {
        @Nullable PcmAudioInput open();
    }

    /** Production {@link PcmAudioInput} backed by an in-process foreground {@link AudioRecord}. */
    private static final class AudioRecordPcmAudioInput implements PcmAudioInput {
        private final AudioRecord mAudioRecord;

        AudioRecordPcmAudioInput(AudioRecord audioRecord) {
            mAudioRecord = audioRecord;
        }

        @Override
        public int read(byte[] buffer, int offset, int length) {
            try {
                return mAudioRecord.read(buffer, offset, length);
            } catch (IllegalStateException e) {
                return AudioRecord.ERROR_INVALID_OPERATION;
            }
        }

        @Override
        public void close() {
            try {
                if (mAudioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
                    mAudioRecord.stop();
                }
            } catch (IllegalStateException e) {
                Log.w(TAG, "AudioRecord.stop() threw exception", e);
            } finally {
                mAudioRecord.release();
            }
        }
    }

    /**
     * Streams foreground {@link PcmAudioInput} bytes into a {@link ParcelFileDescriptor} pipe whose
     * read end is passed to {@link RecognizerIntent#EXTRA_AUDIO_SOURCE}.
     *
     * <p>A dedicated audio thread owns the {@link PcmAudioInput} and the pipe's write end for the
     * whole session: it opens the input, pumps PCM into the pipe, and stops and releases the input
     * once the session closes. The UI thread never makes blocking {@link AudioRecord} Binder calls
     * or touches the input while the audio thread reads from it.
     */
    private static final class AudioPipeSession {
        private final ParcelFileDescriptor mReadFd;
        private final ParcelFileDescriptor mWriteFd;
        private final AtomicBoolean mReadFdClosed = new AtomicBoolean();
        private final AtomicBoolean mClosed = new AtomicBoolean();

        private AudioPipeSession(ParcelFileDescriptor readFd, ParcelFileDescriptor writeFd) {
            mReadFd = readFd;
            mWriteFd = writeFd;
        }

        static @Nullable AudioPipeSession create() {
            try {
                ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createPipe();
                return new AudioPipeSession(pipe[0], pipe[1]);
            } catch (IOException e) {
                Log.w(TAG, "Failed to create speech audio pipe", e);
                return null;
            }
        }

        ParcelFileDescriptor getReadFd() {
            return mReadFd;
        }

        /**
         * Starts the audio thread, which opens a {@link PcmAudioInput} from {@code factory},
         * reports whether it opened to {@code onOpened} via {@code uiThreadExecutor}, and then
         * streams PCM into the pipe until the session closes or the pipe breaks.
         */
        void start(
                PcmAudioInputFactory factory,
                Executor uiThreadExecutor,
                Callback<Boolean> onOpened) {
            Thread audioThread =
                    new Thread(
                            () -> runAudioThread(factory, uiThreadExecutor, onOpened),
                            "SpeechRecognitionAudio");
            audioThread.setDaemon(true);
            audioThread.start();
        }

        private void runAudioThread(
                PcmAudioInputFactory factory,
                Executor uiThreadExecutor,
                Callback<Boolean> onOpened) {
            try (OutputStream writeStream =
                    new ParcelFileDescriptor.AutoCloseOutputStream(mWriteFd)) {
                PcmAudioInput audioInput = factory.open();
                boolean opened = audioInput != null;
                uiThreadExecutor.execute(onOpened.bind(opened));
                if (audioInput == null) return;

                try {
                    byte[] buffer = new byte[AUDIO_READ_BUFFER_BYTES];
                    while (!mClosed.get()) {
                        int bytesRead = audioInput.read(buffer, 0, buffer.length);
                        if (bytesRead <= 0) break;
                        writeStream.write(buffer, 0, bytesRead);
                    }
                } finally {
                    audioInput.close();
                }
            } catch (IOException e) {
                // Expected when the recognizer or session closes the pipe.
            }
        }

        void closeReadFd() {
            if (!mReadFdClosed.compareAndSet(false, true)) return;
            try {
                mReadFd.close();
            } catch (IOException ignored) {
            }
        }

        /**
         * Signals the audio thread to stop and releases Chrome's local read descriptor without
         * blocking. The audio thread stops and releases its {@link PcmAudioInput} after its current
         * read, and a write blocked on a full pipe fails with EPIPE once the recognizer closes its
         * end.
         */
        void close() {
            mClosed.set(true);
            closeReadFd();
        }
    }

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

    @JniType("::content::SpeechRecognizerImplAndroid::UiPeer")
    interface NativeUiPeer extends JniTypeToken {}

    private final Intent mIntent;
    private final RecognitionListener mListener;
    private @Nullable SpeechRecognizer mRecognizer;
    private PcmAudioInputFactory mPcmAudioInputFactory =
            SpeechRecognitionImpl::openSystemPcmAudioInput;
    private Executor mUiThreadExecutor =
            task -> PostTask.postTask(TaskTraits.UI_USER_BLOCKING, task);
    private @Nullable AudioPipeSession mAudioPipeSession;

    private @Nullable JniRawPtr<NativeUiPeer> mNativeSpeechRecognizerImplAndroid;

    // Remember if we are using continuous recognition.
    private boolean mContinuous;

    // Remember if the session was started in single-utterance mode (!continuous), since
    // stopRecognition() clears mContinuous while single-utterance sessions using EXTRA_AUDIO_SOURCE
    // still end on their first onSegmentResults().
    private boolean mSingleUtterance;

    // Whether the active session is streaming audio via RecognizerIntent.EXTRA_AUDIO_SOURCE.
    private boolean mUsingExternalAudioSource;

    // Remember if interim results are requested.
    private boolean mInterimResults;

    // Internal class to handle events from Android's SpeechRecognizer and route them to native.
    class Listener implements RecognitionListener {

        @Override
        public void onBeginningOfSpeech() {
            if (mNativeSpeechRecognizerImplAndroid == null) return;
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
                stopAudioSource();
                if (mNativeSpeechRecognizerImplAndroid == null) return;
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
            int code;

            // Translate Android SpeechRecognizer errors to Web Speech API errors.
            switch (error) {
                case SpeechRecognizer.ERROR_AUDIO:
                case SpeechRecognizer.ERROR_RECOGNIZER_BUSY:
                    code = SpeechRecognitionErrorCode.AUDIO_CAPTURE;
                    break;
                case SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS:
                case SpeechRecognizer.ERROR_TOO_MANY_REQUESTS:
                    code = SpeechRecognitionErrorCode.NOT_ALLOWED;
                    break;
                case SpeechRecognizer.ERROR_NETWORK_TIMEOUT:
                case SpeechRecognizer.ERROR_NETWORK:
                case SpeechRecognizer.ERROR_SERVER:
                case SpeechRecognizer.ERROR_SERVER_DISCONNECTED:
                    code = SpeechRecognitionErrorCode.NETWORK;
                    break;
                case SpeechRecognizer.ERROR_NO_MATCH:
                    code = SpeechRecognitionErrorCode.NO_MATCH;
                    break;
                case SpeechRecognizer.ERROR_SPEECH_TIMEOUT:
                    code = SpeechRecognitionErrorCode.NO_SPEECH;
                    break;
                case SpeechRecognizer.ERROR_LANGUAGE_NOT_SUPPORTED:
                case SpeechRecognizer.ERROR_LANGUAGE_UNAVAILABLE:
                    code = SpeechRecognitionErrorCode.LANGUAGE_NOT_SUPPORTED;
                    break;
                case SpeechRecognizer.ERROR_CANNOT_CHECK_SUPPORT:
                case SpeechRecognizer.ERROR_CANNOT_LISTEN_TO_DOWNLOAD_EVENTS:
                    code = SpeechRecognitionErrorCode.SERVICE_NOT_ALLOWED;
                    break;
                case SpeechRecognizer.ERROR_CLIENT:
                default:
                    code = SpeechRecognitionErrorCode.ABORTED;
                    break;
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
            if (mNativeSpeechRecognizerImplAndroid == null) return;
            if (mState != STATE_IDLE) return;

            if (mAudioPipeSession != null) {
                // By the time onReadyForSpeech fires, the recognition service has already dup'd
                // EXTRA_AUDIO_SOURCE over Binder. Release Chrome's local read descriptor so pipe
                // writes fail fast with EPIPE if the remote recognizer closes its end early.
                mAudioPipeSession.closeReadFd();
            }
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
            boolean deliveredResult = handleResults(bundle, /* provisional= */ false);
            // Single-utterance sessions using RecognizerIntent.EXTRA_AUDIO_SOURCE request a
            // segmented session (see onAudioSourceOpened()), so the finalized utterance arrives
            // here rather than via onResults(). End the session on the first delivered utterance.
            if (deliveredResult && mSingleUtterance && mUsingExternalAudioSource) {
                terminate(SpeechRecognitionErrorCode.NONE);
            }
        }

        @Override
        public void onEndOfSegmentedSession() {
            terminate(SpeechRecognitionErrorCode.NONE);
        }

        @Override
        public void onRmsChanged(float rms) {}

        private boolean handleResults(Bundle bundle, boolean provisional) {
            if (mNativeSpeechRecognizerImplAndroid == null) return false;
            if (provisional && !mInterimResults) return false;

            // Not every bundle carries a transcript: an empty bundle ends a continuous session
            // without segmented session support, and some bundles only carry metadata such as
            // the detected language.
            ArrayList<String> list =
                    bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            if (list == null || list.isEmpty()) return false;

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
            if (TextUtils.isEmpty(results[0])) return false;

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
            return true;
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

    private static boolean hasRecordAudioPermission() {
        return ContextUtils.getApplicationContext()
                        .checkSelfPermission(Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Opens and starts a foreground {@link AudioRecord} on the audio thread. RECORD_AUDIO is
     * checked by startRecognition() before the audio thread starts.
     */
    @SuppressLint("MissingPermission")
    private static @Nullable PcmAudioInput openSystemPcmAudioInput() {
        int minBufferBytes =
                AudioRecord.getMinBufferSize(
                        AUDIO_SAMPLE_RATE_HZ, AUDIO_CHANNEL_CONFIG, AUDIO_ENCODING);
        if (minBufferBytes <= 0) {
            Log.w(TAG, "AudioRecord.getMinBufferSize failed: %d", minBufferBytes);
            return null;
        }

        int bufferSizeBytes = Math.max(minBufferBytes * 2, AUDIO_READ_BUFFER_BYTES);
        AudioRecord audioRecord = null;
        try {
            audioRecord =
                    new AudioRecord(
                            MediaRecorder.AudioSource.VOICE_RECOGNITION,
                            AUDIO_SAMPLE_RATE_HZ,
                            AUDIO_CHANNEL_CONFIG,
                            AUDIO_ENCODING,
                            bufferSizeBytes);
            if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
                Log.w(TAG, "AudioRecord failed to initialize");
                audioRecord.release();
                return null;
            }
            audioRecord.startRecording();
            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                Log.w(TAG, "AudioRecord failed to start recording");
                audioRecord.release();
                return null;
            }
            return new AudioRecordPcmAudioInput(audioRecord);
        } catch (RuntimeException e) {
            Log.w(TAG, "Failed to open in-process AudioRecord", e);
            if (audioRecord != null) {
                audioRecord.release();
            }
            return null;
        }
    }

    /**
     * Closes the in-process audio session, if any (safe no-op when {@code
     * AndroidSpeechRecognitionInProcessAudio} is disabled or fell back to {@link
     * SpeechRecognizer}'s default mic capture). The audio thread then stops and releases Chrome's
     * foreground {@link AudioRecord} within one read. Called unconditionally on every session exit
     * path so the OS microphone privacy indicator turns off promptly.
     */
    private void stopAudioSource() {
        AudioPipeSession session = mAudioPipeSession;
        mAudioPipeSession = null;
        if (session != null) {
            session.close();
        }
    }

    @VisibleForTesting
    SpeechRecognitionImpl(
            JniRawPtr<NativeUiPeer> nativeSpeechRecognizerImplAndroid,
            SpeechRecognizer recognizer) {
        mContinuous = false;
        mSingleUtterance = false;
        mUsingExternalAudioSource = false;
        mInterimResults = false;
        mNativeSpeechRecognizerImplAndroid = nativeSpeechRecognizerImplAndroid;
        mListener = new Listener();
        mIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
        mRecognizer = recognizer;
        mRecognizer.setRecognitionListener(mListener);
    }

    private void destroyRecognizer() {
        stopAudioSource();
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
    }

    // This function destroys everything when recognition is done, taking care to properly tear
    // down by calling On{Sound,Audio}End if corresponding On{Audio,Sound}Start were called.
    private void terminate(int error) {
        JniRawPtr<NativeUiPeer> nativePtr = mNativeSpeechRecognizerImplAndroid;
        if (nativePtr == null) return;
        mNativeSpeechRecognizerImplAndroid = null;

        if (mState == STATE_AWAITING_SPEECH || mState == STATE_CAPTURING_SPEECH) {
            if (mState == STATE_CAPTURING_SPEECH) {
                SpeechRecognitionImplJni.get().onSoundEnd(nativePtr);
            }
            SpeechRecognitionImplJni.get().onAudioEnd(nativePtr);
        }
        mState = STATE_IDLE;

        if (error != SpeechRecognitionErrorCode.NONE) {
            SpeechRecognitionImplJni.get().onRecognitionError(nativePtr, error);
        }

        destroyRecognizer();
        SpeechRecognitionImplJni.get().onRecognitionEnd(nativePtr);
        nativePtr.release();
    }

    @CalledByNative
    @VisibleForTesting
    void onNativeDestroyed() {
        if (mNativeSpeechRecognizerImplAndroid != null) {
            mNativeSpeechRecognizerImplAndroid.release();
            mNativeSpeechRecognizerImplAndroid = null;
        }
        mState = STATE_IDLE;
        destroyRecognizer();
    }

    @CalledByNative
    @VisibleForTesting
    static SpeechRecognitionImpl createSpeechRecognition(
            JniRawPtr<NativeUiPeer> nativeSpeechRecognizerImplAndroid) {
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
        mSingleUtterance = !continuous;
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
        } else {
            mIntent.removeExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                mIntent.removeExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION);
            }
        }

        // Reset any previous session's in-process audio pipe and Intent extras before starting.
        stopAudioSource();
        mUsingExternalAudioSource = false;
        clearAudioSourceExtras();

        // Gate in-process foreground mic capture on Android T+ (where EXTRA_AUDIO_SOURCE was
        // introduced) and the AndroidSpeechRecognitionInProcessAudio Finch killswitch. Since
        // AudioRecord makes blocking Binder calls, the audio thread opens it and the recognizer
        // starts once it reports back, falling back to default mic capture if opening fails.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && ContentFeatureList.isAndroidSpeechRecognitionInProcessAudioEnabled()
                && hasRecordAudioPermission()) {
            AudioPipeSession session = AudioPipeSession.create();
            if (session != null) {
                mAudioPipeSession = session;
                session.start(
                        mPcmAudioInputFactory,
                        mUiThreadExecutor,
                        opened -> onAudioSourceOpened(session, opened));
                return;
            }
        }

        startListening();
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private void onAudioSourceOpened(AudioPipeSession session, boolean opened) {
        // Ignore late reports for a session that was stopped, aborted, or restarted, or whose
        // recognizer was destroyed, while its audio input was opening.
        if (session != mAudioPipeSession || mRecognizer == null) return;

        if (!opened) {
            stopAudioSource();
            startListening();
            return;
        }

        // With EXTRA_AUDIO_SOURCE, the recognition service does not request transient audio focus,
        // so media keeps playing during recognition. This is intended and matches desktop.
        mUsingExternalAudioSource = true;
        mIntent.putExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE, session.getReadFd());
        mIntent.putExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_CHANNEL_COUNT, AUDIO_CHANNEL_COUNT);
        mIntent.putExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_ENCODING, AUDIO_ENCODING);
        mIntent.putExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_SAMPLING_RATE, AUDIO_SAMPLE_RATE_HZ);
        if (mSingleUtterance) {
            // See SINGLE_UTTERANCE_COMPLETE_SILENCE_LENGTH_MS.
            mIntent.putExtra(
                    RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,
                    SINGLE_UTTERANCE_COMPLETE_SILENCE_LENGTH_MS);
            mIntent.putExtra(
                    RecognizerIntent.EXTRA_SEGMENTED_SESSION,
                    RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS);
        }
        startListening();
    }

    private void startListening() {
        SpeechRecognizer recognizer = mRecognizer;
        if (recognizer == null) return;

        try {
            recognizer.startListening(mIntent);
        } catch (SecurityException e) {
            stopAudioSource();
            mUsingExternalAudioSource = false;
            clearAudioSourceExtras();
            Context context = ContextUtils.getApplicationContext();
            String msg = context.getString(R.string.speech_recognition_service_not_found);
            Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
        }
    }

    private void clearAudioSourceExtras() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            mIntent.removeExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE);
            mIntent.removeExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_CHANNEL_COUNT);
            mIntent.removeExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_ENCODING);
            mIntent.removeExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_SAMPLING_RATE);
            mIntent.removeExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS);
            // Keep the segmented session that continuous sessions request.
            if (RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS.equals(
                    mIntent.getStringExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION))) {
                mIntent.removeExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION);
            }
        }
    }

    @CalledByNative
    @VisibleForTesting
    void abortRecognition() {
        if (mRecognizer == null) return;

        stopAudioSource();
        mRecognizer.cancel();
        terminate(SpeechRecognitionErrorCode.ABORTED);
    }

    @CalledByNative
    @VisibleForTesting
    void stopRecognition() {
        if (mRecognizer == null) return;

        if (mState == STATE_IDLE) {
            // Audio capture (onReadyForSpeech) has not started yet, so there is no result to wait
            // for. Cancel and end immediately, matching desktop pre-capture stop behavior.
            stopAudioSource();
            mRecognizer.cancel();
            terminate(SpeechRecognitionErrorCode.NONE);
            return;
        }

        mContinuous = false;
        stopAudioSource();
        mRecognizer.stopListening();
    }

    void setPcmAudioInputFactoryForTesting(PcmAudioInputFactory factory) {
        mPcmAudioInputFactory = factory;
    }

    void setUiThreadExecutorForTesting(Executor executor) {
        mUiThreadExecutor = executor;
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
        void onAudioStart(JniPtr<NativeUiPeer> self);

        void onSoundStart(JniPtr<NativeUiPeer> self);

        void onSoundEnd(JniPtr<NativeUiPeer> self);

        void onAudioEnd(JniPtr<NativeUiPeer> self);

        void onRecognitionResults(
                JniPtr<NativeUiPeer> self,
                String[] results,
                float @Nullable [] scores,
                boolean provisional);

        void onRecognitionError(JniPtr<NativeUiPeer> self, int error);

        void onRecognitionEnd(JniPtr<NativeUiPeer> self);
    }
}
