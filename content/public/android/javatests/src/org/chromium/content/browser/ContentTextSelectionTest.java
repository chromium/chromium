// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.os.SystemClock;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SdkSuppress;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.ChromiumBaseInputConnection;
import org.chromium.content.browser.input.ImeTestUtils;
import org.chromium.content.browser.selection.SelectActionMenuHelper.DefaultItemOrder;
import org.chromium.content.browser.selection.SelectActionMenuHelper.GroupItemOrder;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/** Integration tests for text selection-related behavior. */
@RunWith(ContentJUnit4ClassRunner.class)
public class ContentTextSelectionTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    // Page needs to be long enough for scroll.
    private static final String DATA_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><meta name=\"viewport\"content=\"width=device-width\""
                        + " /></head><body style='height: 1000px'><form"
                        + " action=\"about:blank\"><input id=\"phone_number\" type=\"tel\""
                        + " value=\"01234567891234\" /><input id=\"empty_input_text\" type=\"text\""
                        + " /><input id=\"whitespace_input_text\" type=\"text\" value=\" \""
                        + " /><input id=\"input_text\" type=\"text\" value=\"SampleInputText\""
                        + " /><textarea id=\"textarea\" rows=\"2\""
                        + " cols=\"20\">SampleTextArea</textarea><input id=\"password\""
                        + " type=\"password\" value=\"SamplePassword\" size=\"10\"/><p><span"
                        + " id=\"smart_selection\">1600 Amphitheatre Parkway</span></p><p><span"
                        + " id=\"plain_text_1\">SamplePlainTextOne</span></p><p><span"
                        + " id=\"plain_text_2\">SamplePlainTextTwo</span></p><input"
                        + " id=\"disabled_text\" type=\"text\" disabled value=\"Sample Text\""
                        + " /><div id=\"rich_div\" contentEditable=\"true\" >Rich Editor</div>"
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
        public void selectAroundCaretAck(SelectAroundCaretResult result) {}

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            final SelectionClient.Result result;
            if (shouldSuggest) {
                result = mResult;
            } else {
                result = new SelectionClient.Result();
            }

            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> mResultCallback.onClassified(result));
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

    private static class TestSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
        @Override
        public void modifyDefaultMenuItems(
                List<SelectionMenuItem.Builder> menuItemBuilders,
                boolean isSelectionPassword,
                String selectedText) {
            // No-op because we are testing default menu item ordering with no modifications.
        }

        @Override
        public List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities) {
            List<ResolveInfo> resolveInfos = new ArrayList<>();
            ResolveInfo resolveInfo =
                    createResolveInfoWithActivityInfo("ProcessTextActivity", true);
            resolveInfos.add(resolveInfo);
            return resolveInfos;
        }

        @Override
        public List<SelectionMenuItem> getAdditionalNonSelectionItems() {
            return Arrays.asList(new SelectionMenuItem.Builder("testNonSelectionItem").build());
        }

        @Override
        public List<SelectionMenuItem> getAdditionalTextProcessingItems() {
            return new ArrayList<>();
        }

        private ResolveInfo createResolveInfoWithActivityInfo(
                String activityName, boolean exported) {
            String packageName = "org.chromium.content.browser.ContentTextSelectionTest";

            ActivityInfo activityInfo = new ActivityInfo();
            activityInfo.packageName = packageName;
            activityInfo.name = activityName;
            activityInfo.exported = exported;
            activityInfo.applicationInfo = new ApplicationInfo();
            activityInfo.applicationInfo.flags = ApplicationInfo.FLAG_SYSTEM;

            ResolveInfo resolveInfo =
                    new ResolveInfo() {
                        @Override
                        public CharSequence loadLabel(PackageManager pm) {
                            return "testTextProcessingItem";
                        }
                    };
            resolveInfo.activityInfo = activityInfo;
            return resolveInfo;
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
    @DisabledTest(message = "https://crbug.com/1237513")
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
    @DisabledTest(message = "https://crbug.com/1237513")
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
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterScroll() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "plain_text_1");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
        Assert.assertTrue(mSelectionPopupController.hasSelection());

        View webContentsView = mWebContents.getViewAndroidDelegate().getContainerView();
        float mCurrentX = webContentsView.getWidth() / 2f;
        float mCurrentY = webContentsView.getHeight() / 2f;

        // Perform a scroll.
        TouchCommon.performDrag(
                mActivityTestRule.getActivity(),
                mCurrentX,
                mCurrentX,
                mCurrentY,
                mCurrentY - 100,
                /* stepCount= */ 3, /* duration in ms */
                250);

        // Ensure selection context menu re-appears after scroll ends.
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
    }

    @Test
    @SmallTest
    @Feature({"TextSelection"})
    public void testPastePopupClearedOnScroll() throws Throwable {
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        waitForPastePopupStatus(true);
        waitForSelectActionBarVisible(false);

        View webContentsView = mWebContents.getViewAndroidDelegate().getContainerView();
        float mCurrentX = webContentsView.getWidth() / 2f;
        float mCurrentY = webContentsView.getHeight() / 2f;

        // Perform a scroll.
        TouchCommon.performDrag(
                mActivityTestRule.getActivity(),
                mCurrentX,
                mCurrentX,
                mCurrentY,
                mCurrentY - 100,
                /* stepCount= */ 3, /* duration in ms */
                250);

        // paste popup should be destroyed on scroll.
        waitForPastePopupStatus(false);
        Assert.assertFalse(mSelectionPopupController.isActionModeValid());
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.P) /* getSecondaryAssistItems requires >= P */
    public void testCorrectPasteMenuItemsAddedWhenThereIsNoSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "whitespace_input_text");
        waitForPastePopupStatus(true);
        waitForSelectActionBarVisible(false);
        SelectionMenuGroup[] menuGroups =
                mSelectionPopupController.getMenuItems().toArray(new SelectionMenuGroup[0]);
        // Default and secondary assist item groups are added to the menu.
        Assert.assertEquals(GroupItemOrder.DEFAULT_ITEMS, menuGroups[0].order);
        Assert.assertEquals(GroupItemOrder.SECONDARY_ASSIST_ITEMS, menuGroups[1].order);
        // Default items. Subtracting 1 to adjust the 1-based indices of the DefaultItemOrder
        // constants to the 0-based indices of arrays.
        SelectionMenuItem[] defaultItems = menuGroups[0].items.toArray(new SelectionMenuItem[0]);
        Assert.assertTrue(defaultItems[DefaultItemOrder.PASTE - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.SELECT_ALL - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.CUT - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.COPY - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.PASTE_AS_PLAIN_TEXT - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.SHARE - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.WEB_SEARCH - 1].isEnabled);
        // The additional non selection (secondary assist) menu item we created is
        // added to the menu.
        Assert.assertEquals(
                "testNonSelectionItem",
                menuGroups[1].items.first().getTitle(mActivityTestRule.getActivity()));
        Assert.assertTrue(menuGroups[1].items.first().isEnabled);
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectSelectionMenuItemsAddedForInputSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        // For primary assist item.
        SelectionClient.Result result = new SelectionClient.Result();
        result.label = "Phone";
        result.intent = new Intent();
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());
        mSelectionPopupController.setSelectionClient(client);

        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "phone_number");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
        SelectionMenuGroup[] menuGroups =
                mSelectionPopupController.getMenuItems().toArray(new SelectionMenuGroup[0]);
        // Default, primary assist, and text processing item groups are added to the menu.
        Assert.assertEquals(GroupItemOrder.ASSIST_ITEMS, menuGroups[0].order);
        Assert.assertEquals(GroupItemOrder.DEFAULT_ITEMS, menuGroups[1].order);
        Assert.assertEquals(GroupItemOrder.TEXT_PROCESSING_ITEMS, menuGroups[2].order);

        // Primary assist item we created is added to menu.
        Assert.assertEquals(
                "Phone", menuGroups[0].items.first().getTitle(mActivityTestRule.getActivity()));
        Assert.assertTrue(menuGroups[0].items.first().isEnabled);
        // Default items.
        SelectionMenuItem[] defaultItems = menuGroups[1].items.toArray(new SelectionMenuItem[0]);
        Assert.assertTrue(defaultItems[DefaultItemOrder.CUT - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.COPY - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.SELECT_ALL - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.PASTE - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.PASTE_AS_PLAIN_TEXT - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.SHARE - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.WEB_SEARCH - 1].isEnabled);
        // The text processing menu item we created is added to the menu.
        Assert.assertEquals(
                "testTextProcessingItem",
                menuGroups[2].items.first().getTitle(mActivityTestRule.getActivity()));
        Assert.assertTrue(menuGroups[2].items.first().isEnabled);
        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertFalse(
                menuGroups[2]
                        .items
                        .first()
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectSelectionMenuItemsAddedForPlainTextSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        // For primary assist item.
        SelectionClient.Result result = new SelectionClient.Result();
        result.label = "Map";
        result.intent = new Intent();
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());
        mSelectionPopupController.setSelectionClient(client);

        DOMUtils.longPressNode(mWebContents, "smart_selection");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
        SelectionMenuGroup[] menuGroups =
                mSelectionPopupController.getMenuItems().toArray(new SelectionMenuGroup[0]);
        // Default, primary assist, and text processing item groups are added to the menu.
        Assert.assertEquals(GroupItemOrder.ASSIST_ITEMS, menuGroups[0].order);
        Assert.assertEquals(GroupItemOrder.DEFAULT_ITEMS, menuGroups[1].order);
        Assert.assertEquals(GroupItemOrder.TEXT_PROCESSING_ITEMS, menuGroups[2].order);
        // Primary assist item we created is added to menu.
        Assert.assertEquals(
                "Map", menuGroups[0].items.first().getTitle(mActivityTestRule.getActivity()));
        Assert.assertTrue(menuGroups[0].items.first().isEnabled);
        // Default items.
        SelectionMenuItem[] defaultItems = menuGroups[1].items.toArray(new SelectionMenuItem[0]);
        Assert.assertTrue(defaultItems[DefaultItemOrder.COPY - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.SHARE - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.SELECT_ALL - 1].isEnabled);
        Assert.assertTrue(defaultItems[DefaultItemOrder.WEB_SEARCH - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.CUT - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.PASTE - 1].isEnabled);
        Assert.assertFalse(defaultItems[DefaultItemOrder.PASTE_AS_PLAIN_TEXT - 1].isEnabled);
        // The text processing menu item we created is added to the menu.
        Assert.assertEquals(
                "testTextProcessingItem",
                menuGroups[2].items.first().getTitle(mActivityTestRule.getActivity()));
        Assert.assertTrue(menuGroups[2].items.first().isEnabled);
        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertTrue(
                menuGroups[2]
                        .items
                        .first()
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
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
    @DisabledTest(message = "https://crbug.com/1360509")
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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
    @DisabledTest(message = "crbug.com/1426223")
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSelectionPopupController.getClassificationResult().startAdjust,
                            Matchers.is(0));
                    Criteria.checkThat(
                            mSelectionPopupController.getSelectedText(),
                            Matchers.is("Amphitheatre"));
                });
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "https://crbug.com/1315297")
    public void testPastePopupDismissedOnDestroy() throws Throwable {
        copyStringToClipboard("SampleTextToCopy");
        DOMUtils.longPressNode(mWebContents, "empty_input_text");
        waitForPastePopupStatus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWebContents.destroy();
                });
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
                (ChromiumBaseInputConnection)
                        mActivityTestRule.getImeAdapter().getInputConnectionForTest();
        return ImeTestUtils.runBlockingOnHandlerNoException(
                connection.getHandler(),
                new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return connection.getTextBeforeCursor(length, flags);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"TextSelection", "TextInput"})
    @DisabledTest(message = "https://crbug.com/1217277")
    public void testCursorPositionAfterHidingActionMode() throws Exception {
        DOMUtils.longPressNode(mWebContents, "textarea");
        waitForSelectActionBarVisible(true);
        Assert.assertTrue(mSelectionPopupController.hasSelection());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        Assert.assertEquals(mSelectionPopupController.getSelectedText(), "SampleTextArea");
        hideSelectActionMode();
        waitForSelectActionBarVisible(false);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(getTextBeforeCursor(50, 0), Matchers.is("SampleTextArea"));
                });
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
    @DisabledTest(message = "https://crbug.com/1315299")
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.paste();
                });
    }

    private void selectActionBarSelectAll() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.selectAll();
                });
    }

    private void selectActionBarCut() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.cut();
                });
    }

    private void selectActionBarCopy() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.copy();
                });
    }

    private void selectActionBarSearch() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.search();
                });
    }

    private void selectActionBarShare() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.share();
                });
    }

    private void hideSelectActionMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionPopupController.destroySelectActionMode();
                });
    }

    private void waitForClipboardContents(final String expectedContents) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Context context = mActivityTestRule.getActivity();
                    ClipboardManager clipboardManager =
                            (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = clipboardManager.getPrimaryClip();
                    Criteria.checkThat(clip, Matchers.notNullValue());
                    Criteria.checkThat(clip.getItemCount(), Matchers.is(1));
                    Criteria.checkThat(clip.getItemAt(0).getText(), Matchers.is(expectedContents));
                });
    }

    private void waitForSelectActionBarVisible(final boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSelectionPopupController.isSelectActionBarShowing(),
                            Matchers.is(visible));
                });
    }

    private void setVisibileOnUiThread(final boolean show) {
        final WebContents webContents = mActivityTestRule.getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (show) {
                        webContents.updateWebContentsVisibility(Visibility.VISIBLE);
                    } else {
                        webContents.updateWebContentsVisibility(Visibility.HIDDEN);
                    }
                });
    }

    private void setAttachedOnUiThread(final boolean attached) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewEventSinkImpl viewEventSink =
                            ViewEventSinkImpl.from(mActivityTestRule.getWebContents());
                    viewEventSink.onViewFocusChanged(gainFocus);
                });
    }

    private void copyStringToClipboard(final String string) throws Throwable {
        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        ClipboardManager clipboardManager =
                                (ClipboardManager)
                                        mActivityTestRule
                                                .getActivity()
                                                .getSystemService(Context.CLIPBOARD_SERVICE);
                        ClipData clip = ClipData.newPlainText("test", string);
                        clipboardManager.setPrimaryClip(clip);
                    }
                });
    }

    private void copyHtmlToClipboard(final String plainText, final String htmlText)
            throws Throwable {
        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        ClipboardManager clipboardManager =
                                (ClipboardManager)
                                        mActivityTestRule
                                                .getActivity()
                                                .getSystemService(Context.CLIPBOARD_SERVICE);
                        ClipData clip = ClipData.newHtmlText("html", plainText, htmlText);
                        clipboardManager.setPrimaryClip(clip);
                    }
                });
    }

    private void waitForPastePopupStatus(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSelectionPopupController.isPasteActionModeValid(), Matchers.is(show));
                });
    }

    private void waitForInsertion(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSelectionPopupController.isInsertionForTesting(), Matchers.is(show));
                });
    }
}
