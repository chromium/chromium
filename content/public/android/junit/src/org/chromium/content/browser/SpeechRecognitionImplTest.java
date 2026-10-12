// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSpeechRecognizer;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.media.mojom.SpeechRecognitionErrorCode;

import java.util.ArrayList;
import java.util.Arrays;

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

    private SpeechRecognitionImpl mSpeechRecognition;
    private RecognitionListener mListener;

    @Before
    public void setUp() {
        SpeechRecognitionImplJni.setInstanceForTesting(mNativesMock);
        mSpeechRecognition = new SpeechRecognitionImpl(NATIVE_PTR, mRecognizerMock);
        mListener = mSpeechRecognition.getListenerForTesting();
    }

    private void startRecognition(boolean continuous, boolean interimResults) {
        mSpeechRecognition.startRecognition(LANGUAGE, continuous, interimResults);
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
        SpeechRecognitionImpl impl = SpeechRecognitionImpl.createSpeechRecognition(NATIVE_PTR);
        impl.startRecognition(LANGUAGE, /* continuous= */ false, /* interimResults= */ true);

        SpeechRecognizer recognizer = ShadowSpeechRecognizer.getLatestSpeechRecognizer();
        assertNotNull(recognizer);
        ShadowLooper.idleMainLooper();
        ShadowSpeechRecognizer shadowRecognizer = shadowOf(recognizer);

        shadowRecognizer.triggerOnPartialResults(resultsBundle("hello"));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));

        shadowRecognizer.triggerOnResults(resultsBundle("hello world"));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
        assertTrue(shadowRecognizer.isDestroyed());
    }

    @Test
    public void testPartialResultsAreProvisional() {
        startRecognition(/* continuous= */ false, /* interimResults= */ true);

        mListener.onPartialResults(resultsBundle("hello"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
    }

    @Test
    public void testContinuousPartialResultsAreProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onPartialResults(resultsBundle("hello"));
        mListener.onPartialResults(resultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock, never())
                .onRecognitionResults(anyLong(), any(), any(), /* provisional= */ eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
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
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
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
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"translating"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(true));
    }

    @Test
    public void testPartialResultsIgnoredWhenInterimResultsDisabled() {
        startRecognition(/* continuous= */ true, /* interimResults= */ false);

        mListener.onPartialResults(resultsBundle("hello"));

        verify(mNativesMock, never()).onRecognitionResults(anyLong(), any(), any(), anyBoolean());
    }

    @Test
    public void testFinalPartialResultsAreNotProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        // Without segmented session support, finalized utterances arrive as flagged partial
        // results while the session stays open.
        mListener.onPartialResults(finalPartialResultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
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
                .onRecognitionResults(anyLong(), any(), any(), /* provisional= */ eq(true));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
    }

    @Test
    public void testSegmentResultsAreNotProvisional() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onSegmentResults(resultsBundle("hello world"));

        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
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
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"how are you"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
    }

    @Test
    public void testEndOfSegmentedSessionEndsRecognition() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mListener.onEndOfSegmentedSession();

        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
        verify(mNativesMock, never()).onRecognitionError(anyLong(), anyInt());
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
        inOrder.verify(mNativesMock).onAudioStart(NATIVE_PTR);
        inOrder.verify(mNativesMock).onSoundStart(NATIVE_PTR);
        inOrder.verify(mRecognizerMock).stopListening();
        inOrder.verify(mNativesMock).onSoundEnd(NATIVE_PTR);
        inOrder.verify(mNativesMock).onAudioEnd(NATIVE_PTR);
        inOrder.verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(FULL_CONFIDENCE),
                        eq(false));
        inOrder.verify(mRecognizerMock).destroy();
        inOrder.verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
        verify(mNativesMock, times(1)).onSoundEnd(NATIVE_PTR);
        verify(mNativesMock, times(1)).onAudioEnd(NATIVE_PTR);
        verify(mNativesMock, never()).onRecognitionError(anyLong(), anyInt());
    }

    @Test
    public void testCallbacksAfterAbortAreIgnored() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        mSpeechRecognition.abortRecognition();

        verify(mRecognizerMock).cancel();
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionError(NATIVE_PTR, SpeechRecognitionErrorCode.ABORTED);
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);

        mListener.onPartialResults(resultsBundle("hello"));
        mListener.onSegmentResults(resultsBundle("hello world"));
        mListener.onEndOfSegmentedSession();
        mListener.onResults(resultsBundle("hello world"));

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
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello world"}),
                        aryEq(new float[] {0.9f}),
                        eq(false));
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
    }

    @Test
    public void testEmptyResultsEndRecognitionWithoutResult() {
        startRecognition(/* continuous= */ true, /* interimResults= */ true);

        // Without segmented session support, the end of the session is signaled by an empty
        // results bundle.
        mListener.onResults(new Bundle());

        verify(mNativesMock, never()).onRecognitionResults(anyLong(), any(), any(), anyBoolean());
        verify(mRecognizerMock).destroy();
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
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

        verify(mNativesMock, never()).onRecognitionResults(anyLong(), any(), any(), anyBoolean());
        verify(mNativesMock, never()).onRecognitionEnd(anyLong());
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
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"hello", "hallo"}),
                        aryEq(new float[] {1.0f, 1.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"first", "second"}),
                        aryEq(new float[] {0.9f, 0.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
                        aryEq(new String[] {"one", "two"}),
                        aryEq(new float[] {1.0f, 1.0f}),
                        eq(false));
        verify(mNativesMock)
                .onRecognitionResults(
                        eq(NATIVE_PTR),
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
                        eq(NATIVE_PTR),
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

        verify(mNativesMock, times(1)).onAudioStart(NATIVE_PTR);
        verify(mNativesMock, times(1)).onSoundStart(NATIVE_PTR);
        verify(mNativesMock, never()).onSoundEnd(anyLong());
        verify(mNativesMock, never()).onAudioEnd(anyLong());

        mListener.onEndOfSegmentedSession();
        InOrder inOrder = inOrder(mNativesMock, mRecognizerMock);
        inOrder.verify(mNativesMock).onSoundEnd(NATIVE_PTR);
        inOrder.verify(mNativesMock).onAudioEnd(NATIVE_PTR);
        inOrder.verify(mRecognizerMock).destroy();
        inOrder.verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
    }

    @Test
    public void testStateTransitionsGuardBeginningAndEndOfSpeechAndBlockRestartAfterAudioEnd() {
        startRecognition(/* continuous= */ false, /* interimResults= */ false);

        // Ignored while in STATE_IDLE before onReadyForSpeech.
        mListener.onBeginningOfSpeech();
        mListener.onEndOfSpeech();
        verify(mNativesMock, never()).onSoundStart(anyLong());
        verify(mNativesMock, never()).onSoundEnd(anyLong());
        verify(mNativesMock, never()).onAudioEnd(anyLong());

        // Transition to STATE_AWAITING_SPEECH, then end speech without beginning speech.
        mListener.onReadyForSpeech(new Bundle());
        verify(mNativesMock, times(1)).onAudioStart(NATIVE_PTR);
        mListener.onEndOfSpeech();
        verify(mNativesMock, never()).onSoundEnd(anyLong());
        verify(mNativesMock, times(1)).onAudioEnd(NATIVE_PTR);

        // Any trailing onReadyForSpeech or onBeginningOfSpeech arriving after onEndOfSpeech (while
        // awaiting final result) must not restart audio or sound capture.
        mListener.onReadyForSpeech(new Bundle());
        mListener.onBeginningOfSpeech();
        verify(mNativesMock, times(1)).onAudioStart(NATIVE_PTR);
        verify(mNativesMock, never()).onSoundStart(anyLong());

        // When onError or onResults finally terminates the session, onAudioEnd is not repeated.
        mListener.onError(SpeechRecognizer.ERROR_NO_MATCH);
        verify(mNativesMock, times(1)).onAudioEnd(NATIVE_PTR);
        verify(mNativesMock).onRecognitionError(NATIVE_PTR, SpeechRecognitionErrorCode.NO_MATCH);
        verify(mNativesMock).onRecognitionEnd(NATIVE_PTR);
    }
}
