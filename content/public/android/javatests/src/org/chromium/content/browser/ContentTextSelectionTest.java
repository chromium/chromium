// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.ChromiumBaseInputConnection;
import org.chromium.content.browser.input.ImeTestUtils;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.concurrent.Callable;

/**
 * Integration tests for text selection-related behavior.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class ContentTextSelectionTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"empty_input_text\" type=\"text\" />"
            + "<input id=\"whitespace_input_text\" type=\"text\" value=\" \" />"
            + "<input id=\"input_text\" type=\"text\" value=\"SampleInputText\" />"
            + "<textarea id=\"textarea\" rows=\"2\" cols=\"20\">SampleTextArea</textarea>"
            + "<input id=\"password\" type=\"password\" value=\"SamplePassword\" size=\"10\"/>"
            + "<p><span id=\"smart_selection\">1600 Amphitheatre Parkway</span></p>"
            + "<p><span id=\"plain_text_1\">SamplePlainTextOne</span></p>"
            + "<p><span id=\"plain_text_2\">SamplePlainTextTwo</span></p>"
            + "<input id=\"disabled_text\" type=\"text\" disabled value=\"Sample Text\" />"
            + "<div id=\"rich_div\" contentEditable=\"true\" >Rich Editor</div>"
            + "</form></body></html>");
    private WebContents mWebContents;
    private SelectionPopupControllerImpl mSelectionPopupController;

    private static class TestSelectionClient implements SelectionClient {
        private SelectionClient.Result mResult;
        private SelectionClient.ResultCallback mResultCallback;

        @Override
        public void onSelectionChanged(String selection) {}

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float poxYPix) {}

        @Override
        public void selectWordAroundCaretAck(boolean didSelect, int startAdjust, int endAdjust) {}

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            final SelectionClient.Result result;
            if (shouldSuggest) {
                result = mResult;
            } else {
                result = new SelectionClient.Result();
            }

            PostTask.postTask(
                    UiThreadTaskTraits.DEFAULT, () -> mResultCallback.onClassified(result));
            return true;
        }

        @Override
        public void cancelAllRequests() {}

        public void setResult(SelectionClient.Result result) {
            mResult = result;
        }

        public void setResultCallback(SelectionClient.ResultCallback callback) {
            mResultCallback = callback;
        }
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrl(DATA_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        mWebContents = mActivityTestRule.getWebContents();
        mSelectionPopupController = mActivityTestRule.getSelectionPopupController();
        waitForSelectActionBarVisible(false);
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionClearedAfterLossOfFocus() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);

        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterLossOfFocusIfRequested() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        mSelectionPopupController.setPreserveSelectionOnNextLossOfFocus(true);
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        requestFocusOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Losing focus yet again should properly clear the selection.
        requestFocusOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertFalse(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReshown() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setVisibileOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReattached() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(false);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        setAttachedOnUiThread(true);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "https://crbug.com/980733")
    public void testSelectionPreservedAfterDragAndDrop() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Long press the selected text without release for the following drag.
        long downTime = SystemClock.uptimeMillis();
        DOMUtils.longPressNodeWithoutUp(mWebContents, "plain_text_1", downTime);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Drag to the specified position by a DOM node id.
        int stepCount = 10;
        DOMUtils.dragNodeTo(mWebContents, "plain_text_1", "plain_text_2", stepCount, downTime);
        waitForSelectActionBarVisible(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        DOMUtils.dragNodeEnd(mWebContents, "plain_text_2", downTime);
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mWebContents, "input_text");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.clickNode(mWebContents, "plain_text_2");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        DOMUtils.longPressNode(mWebContents, "plain_text_2");
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingDisabledInput() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        DOMUtils.longPressNode(mWebContents, "disabled_text");
        waitForPastePopupStatus(false);
        waitForInsertion(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNoSelectAllEmptyInput() throws Throwable {
        // Clipboard has to be non-empty for this test to work on SDK < M.
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canSelectAll());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupCanSelectAllNonEmptyInput() throws Throwable {
        // Clipboard has to be non-empty for this test to work on SDK < M.
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "whitespace_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertTrue(mSelectionPopupController.canSelectAll());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O)
    public void testPastePopupPasteAsPlainTextPlainTextRichEditor() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "rich_div");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O)
    public void testPastePopupPasteAsPlainTextPlainTextNormalEditor() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O)
    public void testPastePopupPasteAsPlainTextHtmlTextRichEditor() throws Throwable {
        copyHtmlToClipboard("SampleTextToCopy", "<span style=\"color: red;\">HTML</span>");
        DOMUtils.longPressNode(mWebContents, "rich_div");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertTrue(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O)
    public void testPastePopupPasteAsPlainTextHtmlTextNormalEditor() throws Throwable {
        copyHtmlToClipboard("SampleTextToCopy", "<span style=\"color: red;\">HTML</span>");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        waitForInsertion(true);
        Assert.assertFalse(mSelectionPopupController.canPasteAsPlainText());
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionNormalFlow() throws Throwable {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.label = "Maps";

        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());

        mSelectionPopupController.setSelectionClient(client);

        DOMUtils.longPressNode(mWebContents, "smart_selection");
        waitForSelectActionBarVisible(true);

        Assert.assertEquals(
                "1600 Amphitheatre Parkway", mSelectionPopupController.getSelectedText());

        SelectionClient.Result returnResult = mSelectionPopupController.getClassificationResult();
        Assert.assertEquals(-5, returnResult.startAdjust);
        Assert.assertEquals(8, returnResult.endAdjust);
        Assert.assertEquals("Maps", returnResult.label);
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionReset() throws Throwable {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.label = "Maps";

        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());

        mSelectionPopupController.setSelectionClient(client);

        DOMUtils.longPressNode(mWebContents, "smart_selection");
        waitForSelectActionBarVisible(true);

        Assert.assertEquals(
                "1600 Amphitheatre Parkway", mSelectionPopupController.getSelectedText());

        SelectionClient.Result returnResult = mSelectionPopupController.getClassificationResult();
        Assert.assertEquals(-5, returnResult.startAdjust);
        Assert.assertEquals(8, returnResult.endAdjust);
        Assert.assertEquals("Maps", returnResult.label);

        DOMUtils.clickNode(mWebContents, "smart_selection");

        CriteriaHelper.pollUiThread(Criteria.equals(
                0, () -> mSelectionPopupController.getClassificationResult().startAdjust));
        Assert.assertEquals("Amphitheatre", mSelectionPopupController.getSelectedText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "https://crbug.com/592428")
    public void testPastePopupDismissedOnDestroy() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mWebContents.destroy(); });
        waitForPastePopupStatus(false);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForInput() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isFocusedNodeEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPassword() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isFocusedNodeEditable());
        Assert.assertTrue(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForPlainText() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertFalse(mSelectionPopupController.isFocusedNodeEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testActionBarConfiguredCorrectlyForTextArea() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertTrue(mSelectionPopupController.isFocusedNodeEditable());
        Assert.assertFalse(mSelectionPopupController.isSelectionPassword());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPlainTextCopy() throws Exception {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents("SamplePlainTextOne");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCopy() throws Exception {
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents("SampleInputText");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCopy() throws Exception {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents("SamplePlainTextOne");
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        // Copy option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents("SamplePlainTextOne");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCopy() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCopy();
        waitForClipboardContents("SampleTextArea");
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextCut() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SamplePlainTextOne");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be available for plain text.
        // Hence validating previous Clipboard content.
        waitForClipboardContents("SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputCut() throws Exception {
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
        waitForClipboardContents("SampleInputText");
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordCut() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Cut option won't be there for Password, hence no change in Clipboard
        // Validating with previous Clipboard content
        waitForClipboardContents("SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaCut() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
        waitForClipboardContents("SampleTextArea");
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    @DisabledTest(message = "https://crbug.com/946157")
    public void testSelectActionBarPlainTextSelectAll() throws Exception {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputSelectAll() throws Exception {
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleInputText");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordSelectAll() throws Exception {
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarTextAreaSelectAll() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSelectAll();
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        waitForSelectActionBarVisible(true);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
    }

    private CharSequence getTextBeforeCursor(final int length, final int flags) {
        final ChromiumBaseInputConnection connection =
                (ChromiumBaseInputConnection) mActivityTestRule.getImeAdapter()
                        .getInputConnectionForTest();
        return ImeTestUtils.runBlockingOnHandlerNoException(
                connection.getHandler(), new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return connection.getTextBeforeCursor(length, flags);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"TextSelection", "TextInput"})
    public void testCursorPositionAfterHidingActionMode() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        hideSelectActionMode();
        waitForSelectActionBarVisible(false);
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals("SampleTextArea", new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return getTextBeforeCursor(50, 0);
                    }
                }));
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectActionBarPlainTextPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        // Paste option won't be available for plain text.
        // Hence content won't be changed.
        Assert.assertNotSame(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarInputPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");

        // Select the input field.
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        // Paste into the input field.
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");

        // Ensure the new text matches the pasted text.
        DOMUtils.longPressNode(mWebContents, "input_text");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals("SampleTextToCopy", mSelectionPopupController.getSelectedText());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarPasswordPaste() throws Throwable {
        copyStringToClipboard("SamplePassword2");

        // Select the password field.
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(
                mSelectionPopupController.getSelectedText().length(), "SamplePassword".length());

        // Paste "SamplePassword2" into the password field, replacing
        // "SamplePassword".
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "");

        // Ensure the new text matches the pasted text. Note that we can't
        // actually compare strings as password field selections only provide
        // a placeholder with the correct length.
        DOMUtils.longPressNode(mWebContents, "password");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(
                mSelectionPopupController.getSelectedText().length(), "SamplePassword2".length());
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "crbug.com/592428")
    public void testSelectActionBarTextAreaPaste() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarPaste();
        DOMUtils.clickNode(mWebContents, "plain_text_1");
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextToCopy");
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarSearchAndShareLaunchesNewTask() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarSearch();
        Intent i = mActivityTestRule.getActivity().getLastSentIntent();
        int new_task_flag = Intent.FLAG_ACTIVITY_NEW_TASK;
        Assert.assertEquals(i.getFlags() & new_task_flag, new_task_flag);

        selectActionBarShare();
        i = mActivityTestRule.getActivity().getLastSentIntent();
        Assert.assertEquals(i.getFlags() & new_task_flag, new_task_flag);
    }

    private void selectActionBarPaste() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.paste(); });
    }

    private void selectActionBarSelectAll() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.selectAll(); });
    }

    private void selectActionBarCut() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.cut(); });
    }

    private void selectActionBarCopy() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.copy(); });
    }

    private void selectActionBarSearch() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.search(); });
    }

    private void selectActionBarShare() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mSelectionPopupController.share(); });
    }

    private void hideSelectActionMode() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSelectionPopupController.destroySelectActionMode(); });
    }

    private void waitForClipboardContents(final String expectedContents) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Context context = mActivityTestRule.getActivity();
                ClipboardManager clipboardManager =
                        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = clipboardManager.getPrimaryClip();
                return clip != null && clip.getItemCount() == 1
                        && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
            }
        });
    }

    private void waitForSelectActionBarVisible(final boolean visible) {
        CriteriaHelper.pollUiThread(Criteria.equals(visible, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mSelectionPopupController.isSelectActionBarShowing();
            }
        }));
    }

    private void setVisibileOnUiThread(final boolean show) {
        final WebContents webContents = mActivityTestRule.getWebContents();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (show) {
                webContents.onShow();
            } else {
                webContents.onHide();
            }
        });
    }

    private void setAttachedOnUiThread(final boolean attached) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewEventSinkImpl viewEventSink =
                    ViewEventSinkImpl.from(mActivityTestRule.getWebContents());
            if (attached) {
                viewEventSink.onAttachedToWindow();
            } else {
                viewEventSink.onDetachedFromWindow();
            }
        });
    }

    private void requestFocusOnUiThread(final boolean gainFocus) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewEventSinkImpl viewEventSink =
                    ViewEventSinkImpl.from(mActivityTestRule.getWebContents());
            viewEventSink.onViewFocusChanged(gainFocus);
        });
    }

    private void copyStringToClipboard(final String string) throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newPlainText("test", string);
                clipboardManager.setPrimaryClip(clip);
            }
        });
    }

    private void copyHtmlToClipboard(final String plainText, final String htmlText)
            throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newHtmlText("html", plainText, htmlText);
                clipboardManager.setPrimaryClip(clip);
            }
        });
    }

    private void waitForPastePopupStatus(final boolean show) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mSelectionPopupController.isPastePopupShowing();
            }
        }));
    }

    private void waitForInsertion(final boolean show) {
        CriteriaHelper.pollUiThread(Criteria.equals(show, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mSelectionPopupController.isInsertionForTesting();
            }
        }));
    }
}
