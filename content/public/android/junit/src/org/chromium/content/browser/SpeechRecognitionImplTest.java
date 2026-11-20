// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.content.Intent;
import android.media.AudioFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSpeechRecognizer;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.content.common.ContentInternalFeatures;
import org.chromium.media.mojom.SpeechRecognitionErrorCode;

import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link SpeechRecognitionImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
// ShadowSpeechRecognizer does not intercept SpeechRecognizer on Android V and above.
@Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class SpeechRecognitionImplTest {
    private static final long NATIVE_PTR = 1234L;
    private static final String LANGUAGE = "en-US";
    private static final String EXTRA_DICTATION_MODE = "android.speech.extra.DICTATION_MODE";
    private static final String EXTRA_UNSTABLE_TEXT = "android.speech.extra.UNSTABLE_TEXT";
    private static final String EXTRA_FINAL_RESULT = "final_result";
    private static final float[] FULL_CONFIDENCE = {1.0f};

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SpeechRecognitionImpl.Natives mNativesMock;
    @Mock private SpeechRecognizer mRecognizerMock;

    // Tasks the audio thread posts to the UI thread, run by the test on the main thread.
    private final LinkedBlockingQueue<Runnable> mUiThreadTasks = new LinkedBlockingQueue<>();

    private JniRawPtr<SpeechRecognitionImpl.NativeUiPeer> mNativePtr;
    private SpeechRecognitionImpl mSpeechRecognition;
    private RecognitionListener mListener;

    @Before
    public void setUp() {
        SpeechRecognitionImplJni.setInstanceForTesting(mNativesMock);
        mNativePtr = JniRawPtr.createForTesting(NATIVE_PTR);
        mSpeechRecognition = new SpeechRecognitionImpl(mNativePtr, mRecognizerMock);
        mSpeechRecognition.setUiThreadExecutorForTesting(mUiThreadTasks::add);
        mListener = mSpeechRecognition.getListenerForTesting();
    }

    @After
    public void tearDown() {
        mSpeechRecognition.onNativeDestroyed();
        SpeechRecognitionImplJni.setInstanceForTesting(null);
    }

    private void startRecognition(boolean continuous, boolean interimResults) {
        mSpeechRecognition.startRecognition(LANGUAGE, continuous, interimResults);
        verify(mRecognizerMock).startListening(mSpeechRecognition.getIntentForTesting());
    }

    private static void grantRecordAudioPermission() {
        shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(Manifest.permission.RECORD_AUDIO);
    }

    /** Runs the task the audio thread posted to report whether the audio input opened. */
    private void runUiThreadTaskFromAudioThread() throws InterruptedException {
        Runnable task = mUiThreadTasks.poll(5, TimeUnit.SECONDS);
        assertNotNull(task);
        task.run();
    }

    /**
     * Starts recognition with in-process audio from {@code audioInput} (or failing to open it when
     * null) and verifies the recognizer starts only once the audio thread reports back.
     */
    private void startRecognitionWithAudioInput(
            FakePcmAudioInput audioInput, boolean continuous, boolean interimResults)
            throws InterruptedException {
        grantRecordAudioPermission();
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(() -> audioInput);
        mSpeechRecognition.startRecognition(LANGUAGE, continuous, interimResults);
        verify(mRecognizerMock, never()).startListening(any());

        runUiThreadTaskFromAudioThread();
        verify(mRecognizerMock).startListening(mSpeechRecognition.getIntentForTesting());
    }

    private static Bundle resultsBundle(String... hypotheses) {
        Bundle bundle = new Bundle();
        bundle.putStringArrayList(
                SpeechRecognizer.RESULTS_RECOGNITION, new ArrayList<>(Arrays.asList(hypotheses)));
        return bundle;
    }

    private static Bundle finalPartialResultsBundle(String... hypotheses) {
        Bundle bundle = resultsBundle(hypotheses);
        bundle.putBoolean(EXTRA_FINAL_RESULT, true);
        return bundle;
    }

    private static void putUnstableText(Bundle bundle, String unstableText) {
        bundle.putStringArrayList(
                EXTRA_UNSTABLE_TEXT, new ArrayList<>(Arrays.asList(unstableText)));
    }

    @Test
    public void testCreateSpeechRecognitionWiresShadowRecognizer() {
        JniRawPtr<SpeechRecognitionImpl.NativeUiPeer> nativePtr =
                JniRawPtr.createForTesting(NATIVE_PTR);
        SpeechRecognitionImpl impl = SpeechRecognitionImpl.createSpeechRecognition(nativePtr);
        try {
            impl.startRecognition(LANGUAGE, /* continuous= */ false, /* interimResults= */ true);

            SpeechRecognizer recognizer = ShadowSpeechRecognizer.getLatestSpeechRecognizer();
            assertNotNull(recognizer);
            ShadowLooper.idleMainLooper();
            ShadowSpeechRecognizer shadowRecognizer = shadowOf(recognizer);

            shadowRecognizer.triggerOnPartialResults(resultsBundle("hello"));
            verify(mNativesMock)
                    .onRecognitionResults(
                            eq(nativePtr),
                            aryEq(new String[] {"hello"}),
                            aryEq(FULL_CONFIDENCE),
                            eq(true));

            shadowRecognizer.triggerOnResults(resultsBundle("hello world"));
            verify(mNativesMock)
                    .onRecognitionResults(
                            eq(nativePtr),
                            aryEq(new String[] {"hello world"}),
                            aryEq(FULL_CONFIDENCE),
                            eq(false));
            verify(mNativesMock).onRecognitionEnd(nativePtr);
            assertTrue(shadowRecognizer.isDestroyed());
        } finally {
            impl.onNativeDestroyed();
        }
    }

    @Test
    public void testPartialResultsAreProvisional() {
        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        mListener.onPartialResults(resultsBundle("hello"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock, never()).onRecognitionEnd(any());
    }

    @Test
    public void testContinuousPartialResultsAreProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onPartialResults(resultsBundle("hello"));
        mListener.onPartialResults(resultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock, never())
                .onRecognitionResults(any(), any(), any(), /* provisional= */ eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(any());
    }

    @Test
    public void testPartialResultsIncludeUnstableText() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Bundle englishBundle = resultsBundle("hello");
        putUnstableText(englishBundle, " world");
        mListener.onPartialResults(englishBundle);

        // Non-space-delimited scripts (e.g. CJK) and subword tokens rely on direct concatenation
        // without injecting extra spaces.
        Bundle cjkBundle = resultsBundle("你好");
        putUnstableText(cjkBundle, "世界");
        mListener.onPartialResults(cjkBundle);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"你好世界"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
    }

    @Test
    public void testPartialResultsUnstableTextOnlyWhenStableEmpty() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Bundle bundle = resultsBundle("");
        putUnstableText(bundle, "translating");
        mListener.onPartialResults(bundle);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"translating"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
    }

    @Test
    public void testPartialResultsIgnoredWhenInterimResultsDisabled() {
        startRecognition(/* continuous= */ true, /* interimResults= */ false);

        mListener.onPartialResults(resultsBundle("hello"));

        verify(mNativesMock, never()).onRecognitionResults(any(), any(), any(), anyBoolean());
    }

    @Test
    public void testFinalPartialResultsAreNotProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        // Without segmented session support, finalized utterances arrive as flagged partial
        // results while the session stays open.
        mListener.onPartialResults(finalPartialResultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(any());
        verify(mRecognizerMock, never()).destroy();
    }

    @Test
    public void testContinuousWithoutInterimResultsReportsFinalPartialResults() {
        startRecognition(/* continuous= */ true, /* interimResults= */ false);

        assertFalse(
                mSpeechRecognition
                        .getIntentForTesting()
                        .getBooleanExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true));

        // Provisional partial results are ignored when interimResults is false, while finalized
        // segments flagged with EXTRA_FINAL_RESULT are still delivered.
        mListener.onPartialResults(resultsBundle("hello"));
        mListener.onPartialResults(finalPartialResultsBundle("hello world"));

        verify(mNativesMock, never())
                .onRecognitionResults(any(), any(), any(), /* provisional= */ eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(any());
    }

    @Test
    public void testSegmentResultsAreNotProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onSegmentResults(resultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(any());
        verify(mRecognizerMock, never()).destroy();
    }

    @Test
    public void testUnstableTextIsNotAppendedToFinalResults() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Bundle segmentResults = resultsBundle("hello world");
        putUnstableText(segmentResults, " ignored");
        mListener.onSegmentResults(segmentResults);

        Bundle finalPartialResults = finalPartialResultsBundle("how are you");
        putUnstableText(finalPartialResults, " ignored");
        mListener.onPartialResults(finalPartialResults);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"how are you"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
    }

    @Test
    public void testEndOfSegmentedSessionEndsRecognition() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onEndOfSegmentedSession();

        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
        verify(mNativesMock, never()).onRecognitionError(any(), anyInt());
    }

    @Test
    public void testStopRecognitionLetsSegmentedSessionFinish() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();

        mSpeechRecognition.stopRecognition();
        verify(mRecognizerMock).stopListening();

        mListener.onEndOfSpeech();
        mListener.onSegmentResults(resultsBundle("hello world"));
        mListener.onEndOfSegmentedSession();

        InOrder inOrder = inOrder(mNativesMock, mRecognizerMock);
        inOrder.verify(mNativesMock).onAudioStart(mNativePtr);
        inOrder.verify(mNativesMock).onSoundStart(mNativePtr);
        inOrder.verify(mRecognizerMock).stopListening();
        inOrder.verify(mNativesMock).onSoundEnd(mNativePtr);
        inOrder.verify(mNativesMock).onAudioEnd(mNativePtr);
        inOrder.verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        inOrder.verify(mRecognizerMock).destroy();
        inOrder.verify(mNativesMock).onRecognitionEnd(mNativePtr);
        verify(mNativesMock, times(1)).onSoundEnd(mNativePtr);
        verify(mNativesMock, times(1)).onAudioEnd(mNativePtr);
        verify(mNativesMock, never()).onRecognitionError(any(), anyInt());
    }

    @Test
    public void testCallbacksAfterAbortAreIgnored() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mSpeechRecognition.abortRecognition();

        verify(mRecognizerMock).cancel();
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.ABORTED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);

        mListener.onPartialResults(resultsBundle("hello"));
        mListener.onSegmentResults(resultsBundle("hello world"));
        mListener.onEndOfSegmentedSession();
        mListener.onResults(resultsBundle("hello world"));

        verifyNoMoreInteractions(mNativesMock);
    }

    @Test
    public void testStopRecognitionBeforeReadyForSpeechEndsImmediately() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mSpeechRecognition.stopRecognition();

        verify(mRecognizerMock).cancel();
        verify(mRecognizerMock, never()).stopListening();
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
        verify(mNativesMock, never()).onRecognitionError(any(), anyInt());

        // Any trailing callback from Android's recognizer after termination is ignored.
        mListener.onReadyForSpeech(new Bundle());
        verifyNoMoreInteractions(mNativesMock);
    }

    @Test
    public void testResultsEndRecognition() {
        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        Bundle bundle = resultsBundle("hello world");
        bundle.putFloatArray(SpeechRecognizer.CONFIDENCE_SCORES, new float[] {0.9f});
        mListener.onResults(bundle);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(new float[] {0.9f}),
                        eq(false));
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testEmptyResultsEndRecognitionWithoutResult() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        // Without segmented session support, the end of the session is signaled by an empty
        // results bundle.
        mListener.onResults(new Bundle());

        verify(mNativesMock, never()).onRecognitionResults(any(), any(), any(), anyBoolean());
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testPartialAndSegmentResultsWithoutTranscriptAreIgnored() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Bundle metadataOnly = new Bundle();
        metadataOnly.putString("detected_language", LANGUAGE);
        mListener.onPartialResults(metadataOnly);

        Bundle emptyTranscript = resultsBundle("");
        putUnstableText(emptyTranscript, "");
        mListener.onPartialResults(emptyTranscript);

        mListener.onSegmentResults(new Bundle());
        mListener.onSegmentResults(resultsBundle(""));

        verify(mNativesMock, never()).onRecognitionResults(any(), any(), any(), anyBoolean());
        verify(mNativesMock, never()).onRecognitionEnd(any());
    }

    @Test
    public void testZeroAndNegativeConfidenceScoresAreReportedAsFullConfidence() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Bundle zeroScores = resultsBundle("hello", "hallo");
        zeroScores.putFloatArray(SpeechRecognizer.CONFIDENCE_SCORES, new float[] {0.0f, 0.0f});
        mListener.onSegmentResults(zeroScores);

        Bundle topScoreWithZeroSecondary = resultsBundle("first", "second");
        topScoreWithZeroSecondary.putFloatArray(
                SpeechRecognizer.CONFIDENCE_SCORES, new float[] {0.9f, 0.0f});
        mListener.onSegmentResults(topScoreWithZeroSecondary);

        Bundle negativeSecondaryScore = resultsBundle("one", "two");
        negativeSecondaryScore.putFloatArray(
                SpeechRecognizer.CONFIDENCE_SCORES, new float[] {0.9f, -1.0f});
        mListener.onSegmentResults(negativeSecondaryScore);

        Bundle negativeScores = resultsBundle("world");
        negativeScores.putFloatArray(SpeechRecognizer.CONFIDENCE_SCORES, new float[] {-1.0f});
        mListener.onResults(negativeScores);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello", "hallo"}),
                        aryEq(new float[] {1.0f, 1.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"first", "second"}),
                        aryEq(new float[] {0.9f, 0.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"one", "two"}),
                        aryEq(new float[] {1.0f, 1.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
    }

    @Test
    public void testMismatchedConfidenceScoresAreReportedAsFullConfidence() {
        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        Bundle bundle = resultsBundle("hello", "hallo");
        bundle.putFloatArray(SpeechRecognizer.CONFIDENCE_SCORES, new float[] {0.9f});
        mListener.onResults(bundle);

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello", "hallo"}),
                        aryEq(new float[] {1.0f, 1.0f}),
                        eq(false));
    }

    @Test
    public void testContinuousIntentRequestsSegmentedSession() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertEquals(LANGUAGE, intent.getStringExtra(RecognizerIntent.EXTRA_LANGUAGE));
        assertTrue(intent.getBooleanExtra(EXTRA_DICTATION_MODE, false));
        assertTrue(intent.getBooleanExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, false));
        assertEquals(
                RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS,
                intent.getStringExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertEquals(
                300000,
                intent.getIntExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS, 0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testContinuousIntentBeforeTiramisuRequestsMinSpeechLengthAndPartials() {
        startRecognition(/* continuous= */ true, /* interimResults= */ false);

        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertTrue(intent.getBooleanExtra(EXTRA_DICTATION_MODE, false));
        assertTrue(intent.getBooleanExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, false));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertEquals(
                300000,
                intent.getIntExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS, 0));
    }

    @Test
    public void testNonContinuousIntentDoesNotRequestSegmentedSession() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertEquals(LANGUAGE, intent.getStringExtra(RecognizerIntent.EXTRA_LANGUAGE));
        assertFalse(intent.getBooleanExtra(EXTRA_DICTATION_MODE, true));
        assertFalse(intent.getBooleanExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS));
    }

    @Test
    public void testSegmentedSessionPreservesStateAcrossSegmentsAndFiresSoundEndOnTerminate() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();
        mListener.onEndOfSpeech();
        mListener.onSegmentResults(resultsBundle("first sentence"));

        // Subsequent segments firing onReadyForSpeech/onBeginningOfSpeech/onEndOfSpeech should not
        // emit duplicate start or premature end events.
        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();
        mListener.onEndOfSpeech();
        mListener.onSegmentResults(resultsBundle("second sentence"));

        verify(mNativesMock, times(1)).onAudioStart(mNativePtr);
        verify(mNativesMock, times(1)).onSoundStart(mNativePtr);
        verify(mNativesMock, never()).onSoundEnd(any());
        verify(mNativesMock, never()).onAudioEnd(any());

        mListener.onEndOfSegmentedSession();
        InOrder inOrder = inOrder(mNativesMock, mRecognizerMock);
        inOrder.verify(mNativesMock).onSoundEnd(mNativePtr);
        inOrder.verify(mNativesMock).onAudioEnd(mNativePtr);
        inOrder.verify(mRecognizerMock).destroy();
        inOrder.verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testStateTransitionsGuardBeginningAndEndOfSpeechAndBlockRestartAfterAudioEnd() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        // Ignored while in STATE_IDLE before onReadyForSpeech.
        mListener.onBeginningOfSpeech();
        mListener.onEndOfSpeech();
        verify(mNativesMock, never()).onSoundStart(any());
        verify(mNativesMock, never()).onSoundEnd(any());
        verify(mNativesMock, never()).onAudioEnd(any());

        // Transition to STATE_AWAITING_SPEECH, then end speech without beginning speech.
        mListener.onReadyForSpeech(new Bundle());
        verify(mNativesMock, times(1)).onAudioStart(mNativePtr);
        mListener.onEndOfSpeech();
        verify(mNativesMock, never()).onSoundEnd(any());
        verify(mNativesMock, times(1)).onAudioEnd(mNativePtr);

        // Any trailing onReadyForSpeech or onBeginningOfSpeech arriving after onEndOfSpeech (while
        // awaiting final result) must not restart audio or sound capture.
        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();
        verify(mNativesMock, times(1)).onAudioStart(mNativePtr);
        verify(mNativesMock, never()).onSoundStart(any());

        // When onError or onResults finally terminates the session, onAudioEnd is not repeated.
        mListener.onError(SpeechRecognizer.ERROR_NO_MATCH);
        verify(mNativesMock, times(1)).onAudioEnd(mNativePtr);
        verify(mNativesMock).onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.NO_MATCH);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnNativeDestroyedReleasesWithoutCallingNative() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);
        mListener.onReadyForSpeech(new Bundle());
        verify(mNativesMock).onAudioStart(mNativePtr);

        mSpeechRecognition.onNativeDestroyed();
        verify(mRecognizerMock).destroy();

        mListener.onBeginningOfSpeech();
        mListener.onResults(resultsBundle("hello"));
        verifyNoMoreInteractions(mNativesMock);
    }

    @Test
    public void testOnErrorLanguageNotSupported() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_LANGUAGE_NOT_SUPPORTED);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock)
                .onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.LANGUAGE_NOT_SUPPORTED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnErrorLanguageUnavailable() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_LANGUAGE_UNAVAILABLE);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock)
                .onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.LANGUAGE_NOT_SUPPORTED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnErrorTooManyRequests() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_TOO_MANY_REQUESTS);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.NOT_ALLOWED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnErrorServerDisconnected() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_SERVER_DISCONNECTED);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.NETWORK);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnErrorCannotCheckSupport() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_CANNOT_CHECK_SUPPORT);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock)
                .onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.SERVICE_NOT_ALLOWED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testOnErrorCannotListenToDownloadEvents() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        mListener.onError(SpeechRecognizer.ERROR_CANNOT_LISTEN_TO_DOWNLOAD_EVENTS);

        verify(mRecognizerMock).destroy();
        verify(mNativesMock)
                .onRecognitionError(mNativePtr, SpeechRecognitionErrorCode.SERVICE_NOT_ALLOWED);
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    private static final class FakePcmAudioInput implements SpeechRecognitionImpl.PcmAudioInput {
        private final byte[] mFirstChunk;
        private final CountDownLatch mFirstChunkFlushed = new CountDownLatch(1);
        private final CountDownLatch mClosedLatch = new CountDownLatch(1);
        private final AtomicReference<Thread> mCloseThread = new AtomicReference<>();
        private boolean mSentFirstChunk;

        FakePcmAudioInput(byte[] firstChunk) {
            mFirstChunk = firstChunk;
        }

        @Override
        public int read(byte[] buffer, int offset, int length) {
            if (!mSentFirstChunk) {
                mSentFirstChunk = true;
                int bytesToCopy = Math.min(mFirstChunk.length, length);
                System.arraycopy(mFirstChunk, 0, buffer, offset, bytesToCopy);
                return bytesToCopy;
            }
            // Entering the second read() guarantees AudioPipeSession already wrote mFirstChunk.
            mFirstChunkFlushed.countDown();
            // Like a live microphone, keep delivering (silent) audio until the session closes.
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return 0;
            }
            Arrays.fill(buffer, offset, offset + length, (byte) 0);
            return length;
        }

        @Override
        public void close() {
            if (mCloseThread.compareAndSet(null, Thread.currentThread())) {
                mClosedLatch.countDown();
            }
        }

        boolean awaitFirstChunkFlushed() throws InterruptedException {
            return mFirstChunkFlushed.await(5, TimeUnit.SECONDS);
        }

        boolean awaitClosed() throws InterruptedException {
            return mClosedLatch.await(5, TimeUnit.SECONDS);
        }

        Thread getCloseThread() {
            return mCloseThread.get();
        }
    }

    @Test
    public void testExternalAudioSourceAttachesPipeAndStreamsPcm() throws Exception {
        byte[] expectedPcm = new byte[] {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x08};
        FakePcmAudioInput audioInput = new FakePcmAudioInput(expectedPcm);

        startRecognitionWithAudioInput(
                audioInput, /* continuous= */ false, /* interimResults= */ true);

        Intent intent = mSpeechRecognition.getIntentForTesting();
        ParcelFileDescriptor readFd =
                intent.getParcelableExtra(
                        RecognizerIntent.EXTRA_AUDIO_SOURCE, ParcelFileDescriptor.class);
        assertNotNull(readFd);
        assertEquals(1, intent.getIntExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_CHANNEL_COUNT, -1));
        assertEquals(
                AudioFormat.ENCODING_PCM_16BIT,
                intent.getIntExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_ENCODING, -1));
        assertEquals(
                16000, intent.getIntExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_SAMPLING_RATE, -1));

        // Simulate Binder marshalling dup'ing the descriptor into the recognition service before
        // onReadyForSpeech releases Chrome's local read descriptor.
        try (ParcelFileDescriptor serviceReadFd = readFd.dup();
                FileInputStream inputStream =
                        new FileInputStream(serviceReadFd.getFileDescriptor())) {
            mListener.onReadyForSpeech(new Bundle());
            assertThrows(IllegalStateException.class, readFd::getFd);
            assertTrue(audioInput.awaitFirstChunkFlushed());

            byte[] actualPcm = new byte[expectedPcm.length];
            assertEquals(expectedPcm.length, inputStream.read(actualPcm));
            assertArrayEquals(expectedPcm, actualPcm);

            mSpeechRecognition.stopRecognition();
            verify(mRecognizerMock).stopListening();
            // The audio thread stops and releases the audio input once the session closes.
            assertTrue(audioInput.awaitClosed());
        }
    }

    @Test
    public void testStartRecognitionClearsStaleContinuousAndAudioSourceExtras() throws Exception {
        FakePcmAudioInput audioInput = new FakePcmAudioInput(new byte[] {1, 2});

        startRecognitionWithAudioInput(
                audioInput, /* continuous= */ true, /* interimResults= */ true);
        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_CHANNEL_COUNT));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_ENCODING));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_SAMPLING_RATE));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS));
        // Continuous sessions stay open across pauses rather than using a silence timeout.
        assertFalse(
                intent.hasExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS));

        // Subsequent non-continuous fallback session clears all stale continuous and audio-source
        // extras from the reused Intent and closes the previous pipe session.
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(() -> null);
        mSpeechRecognition.startRecognition(
                LANGUAGE, /* continuous= */ false, /* interimResults= */ false);
        assertTrue(audioInput.awaitClosed());
        runUiThreadTaskFromAudioThread();
        verify(mRecognizerMock, times(2)).startListening(intent);
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_CHANNEL_COUNT));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_ENCODING));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE_SAMPLING_RATE));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS));
        assertFalse(
                intent.hasExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS));
    }

    @Test
    public void testStartRecognitionClearsStaleSingleUtteranceAudioSourceExtras() throws Exception {
        FakePcmAudioInput audioInput = new FakePcmAudioInput(new byte[] {1, 2});

        startRecognitionWithAudioInput(
                audioInput, /* continuous= */ false, /* interimResults= */ false);
        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertTrue(
                intent.hasExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS));
        assertTrue(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));

        // A subsequent continuous fallback session drops the single-utterance silence timeout while
        // keeping its own segmented session.
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(() -> null);
        mSpeechRecognition.startRecognition(
                LANGUAGE, /* continuous= */ true, /* interimResults= */ false);
        assertTrue(audioInput.awaitClosed());
        runUiThreadTaskFromAudioThread();
        verify(mRecognizerMock, times(2)).startListening(intent);
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
        assertFalse(
                intent.hasExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS));
        assertEquals(
                RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS,
                intent.getStringExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
    }

    @Test
    public void testExternalAudioSourceSingleUtteranceTerminatesOnSegmentResults()
            throws Exception {
        FakePcmAudioInput audioInput = new FakePcmAudioInput(new byte[] {1, 2});

        startRecognitionWithAudioInput(
                audioInput, /* continuous= */ false, /* interimResults= */ true);
        assertTrue(audioInput.awaitFirstChunkFlushed());

        // EXTRA_AUDIO_SOURCE disables the recognition service's default silence timeout, so
        // single-utterance sessions restore it and request a segmented session keyed off it.
        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertEquals(
                5000,
                intent.getIntExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS, 0));
        assertEquals(
                RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,
                intent.getStringExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_MINIMUM_LENGTH_MILLIS));

        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();

        // Single-utterance end of speech closes the in-process audio source so the recognizer can
        // finalize the utterance.
        mListener.onEndOfSpeech();
        assertTrue(audioInput.awaitClosed());

        // Metadata-only segment bundles must not prematurely terminate the session before the
        // utterance transcript arrives.
        Bundle metadataOnly = new Bundle();
        metadataOnly.putString("detected_language", LANGUAGE);
        mListener.onSegmentResults(metadataOnly);
        verify(mNativesMock, never()).onRecognitionEnd(any());

        // With the segmented session, the finalized single utterance arrives via
        // onSegmentResults() and ends the session.
        mListener.onSegmentResults(resultsBundle("hello world"));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(mNativePtr),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    @DisableFeatures(ContentInternalFeatures.ANDROID_SPEECH_RECOGNITION_IN_PROCESS_AUDIO)
    public void testExternalAudioSourceDisabledFlagFallsBackToDefaultRecognizer() {
        grantRecordAudioPermission();
        AtomicBoolean factoryCalled = new AtomicBoolean();
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(
                () -> {
                    factoryCalled.set(true);
                    return new FakePcmAudioInput(new byte[] {1, 2});
                });

        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        assertFalse(factoryCalled.get());
        assertTrue(mUiThreadTasks.isEmpty());
        assertFalse(
                mSpeechRecognition
                        .getIntentForTesting()
                        .hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
    }

    @Test
    public void testWithoutRecordAudioPermissionFallsBackToDefaultRecognizer() {
        AtomicBoolean factoryCalled = new AtomicBoolean();
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(
                () -> {
                    factoryCalled.set(true);
                    return new FakePcmAudioInput(new byte[] {1, 2});
                });

        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        assertFalse(factoryCalled.get());
        assertTrue(mUiThreadTasks.isEmpty());
        assertFalse(
                mSpeechRecognition
                        .getIntentForTesting()
                        .hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
    }

    @Test
    public void testExternalAudioSourceOpenFailureFallsBackToDefaultRecognizer() throws Exception {
        startRecognitionWithAudioInput(
                /* audioInput= */ null, /* continuous= */ false, /* interimResults= */ true);

        Intent intent = mSpeechRecognition.getIntentForTesting();
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_AUDIO_SOURCE));
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_SEGMENTED_SESSION));
        assertFalse(
                intent.hasExtra(
                        RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS));

        mListener.onSegmentResults(resultsBundle("segment"));
        verify(mNativesMock, never()).onRecognitionEnd(any());

        mListener.onResults(resultsBundle("final"));
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
    }

    @Test
    public void testExternalAudioSourceOpensAndReleasesAudioInputOffUiThread() throws Exception {
        FakePcmAudioInput audioInput = new FakePcmAudioInput(new byte[] {1, 2});
        AtomicReference<Thread> openThread = new AtomicReference<>();
        grantRecordAudioPermission();
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(
                () -> {
                    openThread.set(Thread.currentThread());
                    return audioInput;
                });

        mSpeechRecognition.startRecognition(
                LANGUAGE, /* continuous= */ true, /* interimResults= */ true);
        verify(mRecognizerMock, never()).startListening(any());
        runUiThreadTaskFromAudioThread();
        verify(mRecognizerMock).startListening(mSpeechRecognition.getIntentForTesting());
        assertNotNull(openThread.get());
        assertNotEquals(Thread.currentThread(), openThread.get());

        // Aborting on the UI thread only signals the audio thread, which stops and releases the
        // audio input itself.
        mSpeechRecognition.abortRecognition();
        assertTrue(audioInput.awaitClosed());
        assertEquals(openThread.get(), audioInput.getCloseThread());
    }

    @Test
    public void testStopRecognitionWhileOpeningAudioInputNeverStartsRecognizer() throws Exception {
        FakePcmAudioInput audioInput = new FakePcmAudioInput(new byte[] {1, 2});
        grantRecordAudioPermission();
        mSpeechRecognition.setPcmAudioInputFactoryForTesting(() -> audioInput);

        mSpeechRecognition.startRecognition(
                LANGUAGE, /* continuous= */ false, /* interimResults= */ true);
        mSpeechRecognition.stopRecognition();
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(mNativePtr);
        verify(mNativesMock, never()).onRecognitionError(any(), anyInt());

        // The audio thread's late report is ignored, and the audio thread releases the input.
        runUiThreadTaskFromAudioThread();
        assertTrue(audioInput.awaitClosed());
        verify(mRecognizerMock, never()).startListening(any());
    }

    private void assertErrorMapsTo(int androidError, int expectedMojoError) {
        reset(mNativesMock);
        SpeechRecognizer recognizerMock = mock(SpeechRecognizer.class);
        JniRawPtr<SpeechRecognitionImpl.NativeUiPeer> nativePtr =
                JniRawPtr.createForTesting(NATIVE_PTR);
        SpeechRecognitionImpl impl = new SpeechRecognitionImpl(nativePtr, recognizerMock);
        try {
            impl.startRecognition(LANGUAGE, /* continuous= */ false, /* interimResults= */ false);

            impl.getListenerForTesting().onError(androidError);

            verify(mNativesMock).onRecognitionError(nativePtr, expectedMojoError);
            verify(recognizerMock).destroy();
            verify(mNativesMock).onRecognitionEnd(nativePtr);
        } finally {
            impl.onNativeDestroyed();
        }
    }

    @Test
    public void testOnErrorMapsBusyAndTerminatesUnknownErrors() {
        assertErrorMapsTo(
                SpeechRecognizer.ERROR_RECOGNIZER_BUSY, SpeechRecognitionErrorCode.AUDIO_CAPTURE);
        assertErrorMapsTo(
                SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS,
                SpeechRecognitionErrorCode.NOT_ALLOWED);
        assertErrorMapsTo(SpeechRecognizer.ERROR_CLIENT, SpeechRecognitionErrorCode.ABORTED);
        assertErrorMapsTo(999, SpeechRecognitionErrorCode.ABORTED);
    }
}
