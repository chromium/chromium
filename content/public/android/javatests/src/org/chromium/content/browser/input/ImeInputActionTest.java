// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.support.test.filters.SmallTest;
import android.view.inputmethod.EditorInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.base.ime.TextInputAction;
import org.chromium.ui.base.ime.TextInputType;

/**
 * IME (input method editor) and text input tests for enterkeyhint attribute.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-experimental-web-platform-features"})
public class ImeInputActionTest {
    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_ACTION_HTML);
    }

    private void checkInputAction(String elementId, int type, int textAction, int editorAction)
            throws Throwable {
        mRule.focusElement(elementId);
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        mRule.waitForKeyboardInputActionStates(
                1, 0, 1, new Integer[] {type}, new Integer[] {textAction});
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();
        mRule.waitForEditorAction(editorAction);
        mRule.performEditorAction(editorAction);
        mRule.waitForEventLogState(type == TextInputType.TEXT
                        ? "keydown(13),keypress(13),keyup(13)"
                        : "keydown(13),keypress(13),keyup(13),selectionchange");
        mRule.clearEventLogs();
        mRule.resetAllStates();
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testShowAndHideInputAction() throws Throwable {
        Assert.assertNotNull(mRule.getInputMethodManagerWrapper().getInputConnection());
        checkInputAction("contenteditable_default", TextInputType.CONTENT_EDITABLE,
                TextInputAction.DEFAULT, EditorInfo.IME_ACTION_NONE);
        checkInputAction("contenteditable_enter", TextInputType.CONTENT_EDITABLE,
                TextInputAction.ENTER, EditorInfo.IME_ACTION_NONE);
        checkInputAction("contenteditable_go", TextInputType.CONTENT_EDITABLE, TextInputAction.GO,
                EditorInfo.IME_ACTION_GO);
        checkInputAction("contenteditable_done", TextInputType.CONTENT_EDITABLE,
                TextInputAction.DONE, EditorInfo.IME_ACTION_DONE);
        checkInputAction("contenteditable_next", TextInputType.CONTENT_EDITABLE,
                TextInputAction.NEXT, EditorInfo.IME_ACTION_NEXT);
        checkInputAction("contenteditable_previous", TextInputType.CONTENT_EDITABLE,
                TextInputAction.PREVIOUS, EditorInfo.IME_ACTION_PREVIOUS);
        checkInputAction("contenteditable_search", TextInputType.CONTENT_EDITABLE,
                TextInputAction.SEARCH, EditorInfo.IME_ACTION_SEARCH);
        checkInputAction("contenteditable_send", TextInputType.CONTENT_EDITABLE,
                TextInputAction.SEND, EditorInfo.IME_ACTION_SEND);
        checkInputAction("textarea_default", TextInputType.TEXT_AREA, TextInputAction.DEFAULT,
                EditorInfo.IME_ACTION_NONE);
        checkInputAction("textarea_enter", TextInputType.TEXT_AREA, TextInputAction.ENTER,
                EditorInfo.IME_ACTION_NONE);
        checkInputAction("textarea_go", TextInputType.TEXT_AREA, TextInputAction.GO,
                EditorInfo.IME_ACTION_GO);
        checkInputAction("textarea_done", TextInputType.TEXT_AREA, TextInputAction.DONE,
                EditorInfo.IME_ACTION_DONE);
        checkInputAction("textarea_next", TextInputType.TEXT_AREA, TextInputAction.NEXT,
                EditorInfo.IME_ACTION_NEXT);
        checkInputAction("textarea_previous", TextInputType.TEXT_AREA, TextInputAction.PREVIOUS,
                EditorInfo.IME_ACTION_PREVIOUS);
        checkInputAction("textarea_search", TextInputType.TEXT_AREA, TextInputAction.SEARCH,
                EditorInfo.IME_ACTION_SEARCH);
        checkInputAction("textarea_send", TextInputType.TEXT_AREA, TextInputAction.SEND,
                EditorInfo.IME_ACTION_SEND);
        checkInputAction("input_enter", TextInputType.TEXT, TextInputAction.ENTER,
                EditorInfo.IME_ACTION_NONE);
        checkInputAction(
                "input_go", TextInputType.TEXT, TextInputAction.GO, EditorInfo.IME_ACTION_GO);
        checkInputAction(
                "input_done", TextInputType.TEXT, TextInputAction.DONE, EditorInfo.IME_ACTION_DONE);
        checkInputAction(
                "input_next", TextInputType.TEXT, TextInputAction.NEXT, EditorInfo.IME_ACTION_NEXT);
        checkInputAction("input_previous", TextInputType.TEXT, TextInputAction.PREVIOUS,
                EditorInfo.IME_ACTION_PREVIOUS);
        checkInputAction("input_search", TextInputType.TEXT, TextInputAction.SEARCH,
                EditorInfo.IME_ACTION_SEARCH);
        checkInputAction(
                "input_send", TextInputType.TEXT, TextInputAction.SEND, EditorInfo.IME_ACTION_SEND);

        // Now verify that focusing a default input shows next action and doesn't generate key
        // presses and focus moves to next node.
        mRule.focusElement("input_text");
        mRule.waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        mRule.waitForKeyboardInputActionStates(1, 0, 1, new Integer[] {TextInputType.TEXT},
                new Integer[] {TextInputAction.DEFAULT});
        mRule.waitForEventLogs("selectionchange");
        mRule.clearEventLogs();
        mRule.waitForEditorAction(EditorInfo.IME_ACTION_NEXT);
        mRule.performEditorAction(EditorInfo.IME_ACTION_NEXT);
        mRule.waitForEventLogState("selectionchange");
        mRule.clearEventLogs();
        mRule.resetAllStates();

        mRule.waitForFocusedElement("input_enter");
    }
}
