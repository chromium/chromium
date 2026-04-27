// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.inputmethod.CorrectionInfo;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AutocorrectManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutocorrectManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private AutocorrectManager mAutocorrectManager;
    @Mock private ImeAdapterImpl mImeAdapterImpl;
    @Mock private CorrectionInfo mCorrectionInfo;

    @Before
    public void setUp() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
    }

    @Test
    public void testHandlePendingCorrectionWhileHasUnderline() {
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        mAutocorrectManager.handlePendingCorrection(mCorrectionInfo);

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    public void testHandlePendingCorrectionWhileHasNoUnderline() {
        mAutocorrectManager.handlePendingCorrection(mCorrectionInfo);

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    public void testMaybeApplyDeferredUnderlineEnterCase() {
        String text = "receive";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length());
    }

    @Test
    public void testMaybeApplyDeferredUnderlinePunctuationCase() {
        String text = "good morning!";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }

    @Test
    public void testMaybeApplyDeferredUnderlineSpaceCase() {
        String text = "receive ";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }

    @Test
    public void testMaybeApplyDeferredUnderlineWhileHasUnderline() {
        mAutocorrectManager.maybeApplyDeferredUnderline();

        verify(mImeAdapterImpl, never()).appendAutocorrectUnderlineSpan(anyInt(), anyInt());
    }

    @Test
    public void testOnCommitTextOrSendKeyEventWhenCounterNotZero() {
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD - 1; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    public void testOnCommitTextOrSendKeyEventWhenCounterZero() {
        // Ensuring that AutocorrectManager has Underline
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", "testing");
        mAutocorrectManager.handlePendingCorrection(correctionInfo);
        mAutocorrectManager.maybeApplyDeferredUnderline();

        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl).clearAllAutocorrectUnderlineSpans();
    }

    @Test
    public void testOnCommitTextOrSendKeyEventWhileHasNoUnderline() {
        for (int i = 0; i < AutocorrectManager.USER_ACTION_CLEAR_UNDERLINE_THRESHOLD; i++) {
            mAutocorrectManager.onCommitTextOrSendKeyEvent();
        }

        verify(mImeAdapterImpl, never()).clearAllAutocorrectUnderlineSpans();
    }
}
