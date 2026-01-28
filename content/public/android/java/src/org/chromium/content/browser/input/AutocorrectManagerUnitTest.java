// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

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

    @Before
    public void setUp() {
        mAutocorrectManager = new AutocorrectManager(mImeAdapterImpl);
    }

    @Test
    public void testMaybeAppendAutocorrectUnderlineSpanEnterCase() {
        String text = "receive";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeAppendAutocorrectUnderlineSpan();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length());
    }

    @Test
    public void testMaybeAppendAutocorrectUnderlineSpanPunctuationCase() {
        String text = "good morning!";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeAppendAutocorrectUnderlineSpan();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }

    @Test
    public void testMaybeAppendAutocorrectUnderlineSpanSpaceCase() {
        String text = "receive ";
        CorrectionInfo correctionInfo = new CorrectionInfo(0, "", text);
        mAutocorrectManager.handlePendingCorrection(correctionInfo);

        mAutocorrectManager.maybeAppendAutocorrectUnderlineSpan();

        verify(mImeAdapterImpl).appendAutocorrectUnderlineSpan(0, text.length() - 1);
    }
}
