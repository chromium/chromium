// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyChar;
import static org.mockito.ArgumentMatchers.anyInt;

import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.textclassifier.TextClassification;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.IntentUtils;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;
import org.chromium.content.browser.input.ChromiumBaseInputConnection;
import org.chromium.content.browser.input.ImeTestUtils;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.PendingSelectionMenu;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestSelectionDropdownMenuDelegate;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
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
        public List<SelectionMenuItem> getAdditionalMenuItems(
                @MenuType int menuType,
                boolean isSelectionPassword,
                boolean isSelectionReadOnly,
                String selectedText) {
            if (selectedText.isEmpty()) {
                return List.of(
                        new SelectionMenuItem.Builder("testNonSelectionItem")
                                .setOrderAndCategory(
                                        0, SelectionMenuItem.ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                                .build());
            }
            return new ArrayList<>();
        }

        @Override
        public List<ResolveInfo> filterTextProcessingActivities(
                @MenuType int menuType, List<ResolveInfo> activities) {
            List<ResolveInfo> resolveInfos = new ArrayList<>();
            ResolveInfo resolveInfo =
                    createResolveInfoWithActivityInfo("ProcessTextActivity", true);
            resolveInfos.add(resolveInfo);
            return resolveInfos;
        }

        @Override
        public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
            return true;
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> WebContentsUtils.simulateEndOfPaintHolding(mWebContents));
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

    private void setUpTestCorrectPasteMenuItemsAddedWhenThereIsNoSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        copyStringToClipboard("SampleTextToCopy");
        // TODO(crbug.com/452918681): Update to use rightClickNode for dropdown tests. Currently,
        //  rightClickNode is fundamentally broken as it doesn't click in the correct place.
        DOMUtils.longPressNode(mWebContents, "whitespace_input_text");
        waitForPastePopupStatus(true);
        waitForSelectActionBarVisible(false);
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectPasteMenuItemsAddedWhenThereIsNoSelection_dropdown() throws Throwable {
        setUpTestCorrectPasteMenuItemsAddedWhenThereIsNoSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.DROPDOWN);

        List<ItemMatcher> matchers =
                List.of(
                        hasId(R.id.select_action_menu_paste),
                        hasId(R.id.select_action_menu_select_all),
                        isDivider(),
                        hasTitle("testNonSelectionItem"));
        TestSelectionDropdownMenuDelegate dropdownDelegate =
                new TestSelectionDropdownMenuDelegate();
        MVCListAdapter.ModelList items = menu.getMenuAsDropdown(dropdownDelegate);
        verifyMenu(items, matchers, dropdownDelegate);
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectPasteMenuItemsAddedWhenThereIsNoSelection_floating() throws Throwable {
        setUpTestCorrectPasteMenuItemsAddedWhenThereIsNoSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.FLOATING);

        List<ItemMatcher> matchers =
                List.of(
                        hasId(R.id.select_action_menu_paste),
                        hasId(R.id.select_action_menu_select_all),
                        hasTitle("testNonSelectionItem"));
        ArrayList<MenuItem> actualItems = new ArrayList<>();
        Menu fakeMenu = createFakeMenu(actualItems);
        menu.getMenuAsActionMode(fakeMenu);
        verifyMenu(actualItems, matchers);
    }

    private void setUpTestCorrectSelectionMenuItemsAddedForInputSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        // For primary assist item.
        SelectionClient.Result result = new SelectionClient.Result();
        result.textClassification = createSingleActionTextClassification("Phone");
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());
        mSelectionPopupController.setSelectionClient(client);

        copyStringToClipboard("SampleTextToCopy");
        // TODO(crbug.com/452918681): Update to use rightClickNode for dropdown tests. Currently,
        //  rightClickNode is fundamentally broken as it doesn't click in the correct place.
        DOMUtils.longPressNode(mWebContents, "phone_number");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectSelectionMenuItemsAddedForInputSelection_dropdown() throws Throwable {
        setUpTestCorrectSelectionMenuItemsAddedForInputSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.DROPDOWN);

        List<ItemMatcher> matchers =
                List.of(
                        hasTitle("Phone"),
                        isDivider(),
                        hasId(R.id.select_action_menu_cut),
                        hasId(R.id.select_action_menu_copy),
                        hasId(R.id.select_action_menu_paste),
                        hasId(R.id.select_action_menu_select_all),
                        isDivider(),
                        hasTitle("testTextProcessingItem"));
        TestSelectionDropdownMenuDelegate dropdownDelegate =
                new TestSelectionDropdownMenuDelegate();
        MVCListAdapter.ModelList items = menu.getMenuAsDropdown(dropdownDelegate);
        verifyMenu(items, matchers, dropdownDelegate);
        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertFalse(
                menu.getMenuItemsForTesting()
                        .get(menu.getMenuItemsForTesting().size() - 1)
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    public void testCorrectSelectionMenuItemsAddedForInputSelection_floating() throws Throwable {
        setUpTestCorrectSelectionMenuItemsAddedForInputSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.FLOATING);

        List<ItemMatcher> matchers =
                List.of(
                        hasTitle("Phone"),
                        hasId(R.id.select_action_menu_cut),
                        hasId(R.id.select_action_menu_copy),
                        hasId(R.id.select_action_menu_paste),
                        hasId(R.id.select_action_menu_select_all),
                        hasTitle("testTextProcessingItem"));
        ArrayList<MenuItem> actualItems = new ArrayList<>();
        Menu fakeMenu = createFakeMenu(actualItems);
        menu.getMenuAsActionMode(fakeMenu);
        verifyMenu(actualItems, matchers);
        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertFalse(
                menu.getMenuItemsForTesting()
                        .get(menu.getMenuItemsForTesting().size() - 1)
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
    }

    private void setUpTestCorrectSelectionMenuItemsAddedForPlainTextSelection() throws Throwable {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        mSelectionPopupController.setSelectionActionMenuDelegate(selectionActionMenuDelegate);
        // For primary assist item.
        SelectionClient.Result result = new SelectionClient.Result();
        result.textClassification = createSingleActionTextClassification("Map");
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mSelectionPopupController.getResultCallback());
        mSelectionPopupController.setSelectionClient(client);

        // TODO(crbug.com/452918681): Update to use rightClickNode for dropdown tests. Currently,
        //  rightClickNode is fundamentally broken as it doesn't click in the correct place.
        DOMUtils.longPressNode(mWebContents, "smart_selection");
        waitForSelectActionBarVisible(true);
        waitForPastePopupStatus(false);
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    // TODO(crbug.com/385205045) Re-enable on automotive devices once fixed / made less flaky on
    // auto.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testCorrectSelectionMenuItemsAddedForPlainTextSelection_dropdown()
            throws Throwable {
        setUpTestCorrectSelectionMenuItemsAddedForPlainTextSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.DROPDOWN);
        boolean shareAllowed =
                mSelectionPopupController.isSelectActionModeAllowed(
                        ActionModeCallbackHelper.MENU_ITEM_SHARE);
        boolean webSearchAllowed =
                mSelectionPopupController.isSelectActionModeAllowed(
                        ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH);

        // Map | Copy [Share] Select All [Web Search] | testTextProcessingItem
        ArrayList<ItemMatcher> matchers = new ArrayList<>();
        matchers.add(hasTitle("Map"));
        matchers.add(isDivider());
        matchers.add(hasId(R.id.select_action_menu_copy));
        if (shareAllowed) matchers.add(hasId(R.id.select_action_menu_share));
        matchers.add(hasId(R.id.select_action_menu_select_all));
        if (webSearchAllowed) matchers.add(hasId(R.id.select_action_menu_web_search));
        matchers.add(isDivider());
        // The text processing menu item we created is added to the menu.
        matchers.add(hasTitle("testTextProcessingItem"));

        TestSelectionDropdownMenuDelegate dropdownDelegate =
                new TestSelectionDropdownMenuDelegate();
        MVCListAdapter.ModelList items = menu.getMenuAsDropdown(dropdownDelegate);
        verifyMenu(items, matchers, dropdownDelegate);

        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertTrue(
                menu.getMenuItemsForTesting()
                        .get(menu.getMenuItemsForTesting().size() - 1)
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
    }

    @Test
    @MediumTest
    @Feature({"TextSelection"})
    // TODO(crbug.com/385205045) Re-enable on automotive devices once fixed / made less flaky on
    // auto.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testCorrectSelectionMenuItemsAddedForPlainTextSelection_floating()
            throws Throwable {
        setUpTestCorrectSelectionMenuItemsAddedForPlainTextSelection();
        PendingSelectionMenu menu =
                mSelectionPopupController.getPendingSelectionMenu(MenuType.FLOATING);
        boolean shareAllowed =
                mSelectionPopupController.isSelectActionModeAllowed(
                        ActionModeCallbackHelper.MENU_ITEM_SHARE);
        boolean webSearchAllowed =
                mSelectionPopupController.isSelectActionModeAllowed(
                        ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH);

        // Map Copy [Share] Select All [Web Search] testTextProcessingItem
        ArrayList<ItemMatcher> matchers = new ArrayList<>();
        matchers.add(hasTitle("Map"));
        matchers.add(hasId(R.id.select_action_menu_copy));
        if (shareAllowed) matchers.add(hasId(R.id.select_action_menu_share));
        matchers.add(hasId(R.id.select_action_menu_select_all));
        if (webSearchAllowed) matchers.add(hasId(R.id.select_action_menu_web_search));
        // The text processing menu item we created is added to the menu.
        matchers.add(hasTitle("testTextProcessingItem"));

        ArrayList<MenuItem> actualItems = new ArrayList<>();
        Menu fakeMenu = createFakeMenu(actualItems);
        menu.getMenuAsActionMode(fakeMenu);
        verifyMenu(actualItems, matchers);

        // Check correct processText intent state is sent to 3rd party apps.
        Assert.assertTrue(
                menu.getMenuItemsForTesting()
                        .get(menu.getMenuItemsForTesting().size() - 1)
                        .intent
                        .getBooleanExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, false));
    }

    @Test
    @SmallTest
    @Feature({"TextInput"})
    @DisabledTest(message = "https://crbug.com/440474993")
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
        result.textClassification = createSingleActionTextClassification("Maps");

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
        Assert.assertEquals("Maps", returnResult.textClassification.getActions().get(0).getTitle());
    }

    @Test
    @MediumTest
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionReset() throws Throwable {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.textClassification = createSingleActionTextClassification("Maps");

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
        Assert.assertEquals("Maps", returnResult.textClassification.getActions().get(0).getTitle());

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
        Assert.assertEquals("SamplePlainTextOne", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("SampleInputText", mSelectionPopupController.getSelectedText());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals("", mSelectionPopupController.getSelectedText());
        waitForClipboardContents("SampleInputText");
        Assert.assertEquals("", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("SampleTextArea", mSelectionPopupController.getSelectedText());
        Assert.assertTrue(mSelectionPopupController.isActionModeValid());
        selectActionBarCut();
        waitForSelectActionBarVisible(false);
        Assert.assertEquals("", mSelectionPopupController.getSelectedText());
        waitForClipboardContents("SampleTextArea");
        Assert.assertEquals("", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("SampleInputText", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("SampleTextArea", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("SampleTextArea", mSelectionPopupController.getSelectedText());
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
        Assert.assertEquals("", mSelectionPopupController.getSelectedText());

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
        Assert.assertEquals("SampleTextToCopy", mSelectionPopupController.getSelectedText());
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

    private TextClassification createSingleActionTextClassification(String title) {
        Icon actionIcon = Icon.createWithBitmap(Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888));
        PendingIntent intent =
                PendingIntent.getBroadcast(
                        mActivityTestRule.getActivity(),
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(false));
        RemoteAction action = new RemoteAction(actionIcon, title, "This is a menu item", intent);
        return new TextClassification.Builder().addAction(action).build();
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

    // An interface used to check some property of a SelectionMenuItem. To add a new matcher, create
    // a helper method below that returns an implementation. A call to verifyMenu will check all of
    // the matchers for a given menu.
    private interface ItemMatcher {
        // Check whether the given item matches the criteria. A value of null means the item is a
        // divider.
        boolean check(@Nullable SelectionMenuItem item);
    }

    // Non-static as it needs access to mActivityTestRule.
    private ItemMatcher hasTitle(CharSequence title) {
        return item ->
                item != null
                        && TextUtils.equals(item.getTitle(mActivityTestRule.getActivity()), title);
    }

    private static ItemMatcher hasId(int id) {
        return item -> item != null && item.id == id;
    }

    private static ItemMatcher isDivider() {
        return Objects::isNull;
    }

    private static void verifyMenu(
            MVCListAdapter.ModelList actual,
            List<ItemMatcher> expected,
            TestSelectionDropdownMenuDelegate delegate) {
        Assert.assertEquals(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); i++) {
            SelectionMenuItem menuItem = delegate.getMinimalMenuItem(actual.get(i).model);
            Assert.assertTrue(
                    expected.get(i)
                            .check(
                                    actual.get(i).type
                                                    == TestSelectionDropdownMenuDelegate
                                                            .ListMenuItemType.DIVIDER
                                            ? null
                                            : menuItem));
        }
    }

    private static void verifyMenu(List<MenuItem> menuItems, List<ItemMatcher> expected) {
        Assert.assertEquals(expected.size(), menuItems.size());
        for (int i = 0; i < expected.size(); i++) {
            SelectionMenuItem menuItem =
                    new SelectionMenuItem.Builder(menuItems.get(i).getTitle())
                            .setId(menuItems.get(i).getItemId())
                            .build();
            Assert.assertTrue(expected.get(i).check(menuItem));
        }
    }

    /**
     * Create a fake Android Menu that can be passed to PendingSelectionMenu#getMenuAsActionMode.
     *
     * @param itemList A list of MenuItems to populate as items are added.
     * @return the fake menu.
     */
    private static Menu createFakeMenu(ArrayList<MenuItem> itemList) {
        Menu menu = Mockito.spy(Menu.class);
        Mockito.doAnswer(
                        i -> {
                            MenuItem ret = Mockito.spy(MenuItem.class);
                            Mockito.doReturn(i.getArguments()[1]).when(ret).getItemId();
                            Mockito.doReturn(i.getArguments()[3]).when(ret).getTitle();
                            // Mock out the builder methods so we don't break chaining.
                            Mockito.doReturn(ret).when(ret).setShowAsActionFlags(anyInt());
                            Mockito.doReturn(ret).when(ret).setIcon(any());
                            Mockito.doReturn(ret).when(ret).setContentDescription(any());
                            Mockito.doReturn(ret).when(ret).setIntent(any());
                            Mockito.doReturn(ret).when(ret).setAlphabeticShortcut(anyChar());
                            itemList.add(ret);
                            return ret;
                        })
                .when(menu)
                .add(anyInt(), anyInt(), anyInt(), any());
        return menu;
    }
}
