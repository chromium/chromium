// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.inputmethod.CorrectionInfo;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.common.ContentFeatures;

/** Unit tests for {@link AutocorrectManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocorrectManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private AutocorrectManager mAutocorrectManager;
    @Mock private ImeAdapterImpl mImeAdapterImpl;
    @Mock private CorrectionInfo mCorrectionInfo;

    // V1 Strategy Tests

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_HandlePendingCorrectionWhileHasUnderline() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        mAutocorrectManager.handlePendingCorrection(mCorrectionInfo);

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_HandlePendingCorrectionWhileHasNoUnderline() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        mAutocorrectManager.handlePendingCorrection(mCorrectionInfo);

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_ApplyDeferredUnderlineEnterCase() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        String text = "receive";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length());
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_ApplyDeferredUnderlinePunctuationCase() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        String text = "good morning!";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_ApplyDeferredUnderlineSpaceCase() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        String text = "receive ";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_ApplyDeferredUnderlineWhileHasUnderline() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl, never()).appendAutocorrectUnderlineSpan(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_OnCommitTextOrSendKeyEventWhenCounterNotZero() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V1 - 1; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV1Strategy_OnCommitTextOrSendKeyEventWhenCounterZero() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V1; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    // V2 Strategy Tests

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_AppliesUnderlineImmediately() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "new");

        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        // Underline should be applied immediately in V2.
        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, 3);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        // Should not be called again.
        verify(mImeAdapterImpl, times(1)).appendAutocorrectUnderlineSpan(anyInt(), anyInt());
    }

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_HandlePendingCorrectionInitializesCounter() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");

        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, 7);

        // Counter should be at 4 now because V2 strategy starts at
        // USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2 (4).
        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_ApplyUnderlinePunctuationCase() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "good");

        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, 4);
    }

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_ApplyUnderlineSpaceCase() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);

        mAutocorrectManager.handlePendingCorrection(new CorrectionInfo(0, "", "good "));

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, 4);
    }

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_OnCommitTextOrSendKeyEventWhenCounterNotZero() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");

        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2 - 1; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    @DisableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE)
    @EnableFeatures(ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2)
    public void testV2Strategy_OnCommitTextOrSendKeyEventWhenCounterZero() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");

        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V2; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    // General Tests

    @Test
    @DisableFeatures({
        ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE,
        ContentFeatures.ANDROID_PK_AUTOCORRECT_UNDERLINE_V2
    })
    public void testOnCommitTextOrSendKeyEventWhileHasNoUnderline() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD_V1; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }
}
