// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputConnection;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;

import java.util.concurrent.Callable;

/** Integration tests for text input for Android L (or above) features. */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(ImeTest.IME_BATCH)
public class ImeLollipopTest {
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
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/1153705")
    public void testUpdateCursorAnchorInfo() throws Throwable {
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_MONITOR);

        // In "MONITOR" mode, the change should be notified.
        mRule.setComposingText("ab", 1);
        waitForUpdateCursorAnchorInfoComposingText("ab");

        CursorAnchorInfo info = mRule.getInputMethodManagerWrapper().getLastCursorAnchorInfo();
        Assert.assertEquals(0, info.getComposingTextStart());
        Assert.assertNotNull(info.getCharacterBounds(0));
        Assert.assertNotNull(info.getCharacterBounds(1));
        Assert.assertNull(info.getCharacterBounds(2));

        // Should be notified not only once. Further change should be sent, too.
        mRule.setComposingText("abcd", 1);
        waitForUpdateCursorAnchorInfoComposingText("abcd");

        info = mRule.getInputMethodManagerWrapper().getLastCursorAnchorInfo();
        Assert.assertEquals(0, info.getComposingTextStart());
        Assert.assertNotNull(info.getCharacterBounds(0));
        Assert.assertNotNull(info.getCharacterBounds(1));
        Assert.assertNotNull(info.getCharacterBounds(2));
        Assert.assertNotNull(info.getCharacterBounds(3));
        Assert.assertNull(info.getCharacterBounds(4));

        // In "IMMEDIATE" mode, even when there's no change, we should be notified at least once.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRule.getInputMethodManagerWrapper().clearLastCursorAnchorInfo();
                });
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_IMMEDIATE);
        waitForUpdateCursorAnchorInfoComposingText("abcd");

        mRule.setComposingText("abcde", 2);
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_IMMEDIATE);
        waitForUpdateCursorAnchorInfoComposingText("abcde");
    }

    private void requestCursorUpdates(final int cursorUpdateMode) throws Exception {
        final InputConnection connection = mRule.getConnection();
        mRule.runBlockingOnImeThread(
                new Callable<Void>() {
                    @Override
                    public Void call() {
                        connection.requestCursorUpdates(cursorUpdateMode);
                        return null;
                    }
                });
    }

    private void waitForUpdateCursorAnchorInfoComposingText(final String expected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    CursorAnchorInfo info =
                            mRule.getInputMethodManagerWrapper().getLastCursorAnchorInfo();
                    if (info != null) {
                        Criteria.checkThat(info.getComposingText(), Matchers.notNullValue());
                    }

                    String actual = (info == null ? "" : info.getComposingText().toString());
                    Criteria.checkThat(actual, Matchers.is(expected));
                });
    }
}
