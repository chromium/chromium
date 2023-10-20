// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputConnection;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Matchers;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

import java.util.concurrent.Callable;

/**
 * Test for {@link CursorAnchorInfoController} behavior specific to the insertion marker bounds
 * being sent when there's a caret on screen.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-features=HiddenSelectionBounds"})
@Batch(Batch.PER_CLASS)
public class InsertionMarkerTest {
    @Rule public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
    }

    @After
    public void tearDown() throws Exception {
        mRule.getActivity().finish();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1441435")
    public void boundsSentOnFocus() throws Exception {
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_MONITOR);

        mRule.focusElement("input_text");
        waitForCursorAnchorInfoToHaveInsertionBounds();
    }

    @Test
    @MediumTest
    public void boundsSentOnTyping() throws Exception {
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_MONITOR);

        mRule.focusElement("input_text");
        mRule.commitText("hello", 5);
        waitForCursorAnchorInfoToHaveInsertionBounds();
    }

    private void requestCursorUpdates(int cursorUpdateMode) throws Exception {
        InputConnection connection = mRule.getConnection();
        mRule.runBlockingOnImeThread(
                (Callable<Void>)
                        () -> {
                            connection.requestCursorUpdates(cursorUpdateMode);
                            return null;
                        });
    }

    private void waitForCursorAnchorInfoToHaveInsertionBounds() {
        CriteriaHelper.pollUiThread(
                () -> {
                    CursorAnchorInfo info =
                            mRule.getInputMethodManagerWrapper().getLastCursorAnchorInfo();
                    Criteria.checkThat(info, Matchers.notNullValue());

                    Assert.assertFalse(Float.isNaN(info.getInsertionMarkerTop()));
                    Assert.assertFalse(Float.isNaN(info.getInsertionMarkerBottom()));
                    Assert.assertFalse(Float.isNaN(info.getInsertionMarkerHorizontal()));
                });
    }
}
