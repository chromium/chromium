// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content.R;
import org.chromium.content_public.browser.PendingSelectionMenu;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link SelectActionMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectActionMenuHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private SelectActionMenuHelper.TextSelectionCapabilitiesDelegate mDelegate;
    @Mock private Context mContext;
    @Mock private TypedArray mTypedArray;
    @Mock private PackageManager mPackageManager;

    // Passing a null delegate will just build the items as they are with no customization.
    private final SelectActionMenuHelper.ItemBuilder mBuilder =
            new SelectActionMenuHelper.ItemBuilder(
                    new TestSelectionActionMenuDelegate(), MenuType.DROPDOWN, false, "");

    private static final Map<Integer, String> ID_MAPPING =
            Map.of(
                    R.id.select_action_menu_cut, "select_action_menu_cut",
                    R.id.select_action_menu_copy, "select_action_menu_copy",
                    android.R.id.paste, "paste",
                    android.R.id.pasteAsPlainText, "pasteAsPlainText",
                    R.id.select_action_menu_share, "select_action_menu_share",
                    R.id.select_action_menu_select_all, "select_action_menu_select_all",
                    R.id.select_action_menu_web_search, "select_action_menu_web_search");

    private static class TestSelectionActionMenuDelegate implements SelectionActionMenuDelegate {

        @Override
        public @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
            return new @DefaultItem int[] {
                DefaultItem.CUT,
                DefaultItem.COPY,
                DefaultItem.PASTE,
                DefaultItem.PASTE_AS_PLAIN_TEXT,
                DefaultItem.SELECT_ALL,
                DefaultItem.SHARE,
                DefaultItem.WEB_SEARCH
            };
        }

        @Override
        public List<SelectionMenuItem> getAdditionalMenuItems(
                @MenuType int menuType,
                boolean isSelectionPassword,
                boolean isSelectionReadOnly,
                String selectedText) {
            return new ArrayList<>();
        }

        @Override
        public List<ResolveInfo> filterTextProcessingActivities(
                @MenuType int menuType, List<ResolveInfo> activities) {
            List<ResolveInfo> updatedSupportedItems = new ArrayList<>();
            List<String> splitTextManagerApps = Arrays.asList("ProcessTextActivity2");
            for (int i = 0; i < activities.size(); i++) {
                ResolveInfo resolveInfo = activities.get(i);
                if (resolveInfo.activityInfo == null
                        || !splitTextManagerApps.contains(resolveInfo.activityInfo.packageName)) {
                    continue;
                }
                updatedSupportedItems.add(resolveInfo);
            }
            return updatedSupportedItems;
        }

        @Override
        public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
            return true;
        }
    }

    @Before
    public void setUp() {
        // Used to mock out getting menu item icons.
        when(mContext.obtainStyledAttributes(any(int[].class))).thenReturn(mTypedArray);
        when(mTypedArray.getResourceId(anyInt(), anyInt())).thenReturn(0);

        setCapabilities(
                Set.of(
                        DefaultItem.CUT,
                        DefaultItem.COPY,
                        DefaultItem.PASTE,
                        DefaultItem.SELECT_ALL,
                        DefaultItem.WEB_SEARCH,
                        DefaultItem.PASTE_AS_PLAIN_TEXT,
                        DefaultItem.SHARE));
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrder_floating() {
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.FLOATING, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertIdsAreEqual(R.id.select_action_menu_cut, menuItems.get(0).id);
        assertIdsAreEqual(R.id.select_action_menu_copy, menuItems.get(1).id);
        assertIdsAreEqual(android.R.id.paste, menuItems.get(2).id);
        assertIdsAreEqual(android.R.id.pasteAsPlainText, menuItems.get(3).id);
        assertIdsAreEqual(R.id.select_action_menu_share, menuItems.get(4).id);
        assertIdsAreEqual(R.id.select_action_menu_select_all, menuItems.get(5).id);
        assertIdsAreEqual(R.id.select_action_menu_web_search, menuItems.get(6).id);
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrder_dropdown() {
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.DROPDOWN, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertIdsAreEqual(R.id.select_action_menu_cut, menuItems.get(0).id);
        assertIdsAreEqual(R.id.select_action_menu_copy, menuItems.get(1).id);
        assertIdsAreEqual(android.R.id.paste, menuItems.get(2).id);
        assertIdsAreEqual(android.R.id.pasteAsPlainText, menuItems.get(3).id);
        assertIdsAreEqual(R.id.select_action_menu_share, menuItems.get(4).id);
        assertIdsAreEqual(R.id.select_action_menu_select_all, menuItems.get(5).id);
        assertIdsAreEqual(R.id.select_action_menu_web_search, menuItems.get(6).id);
    }

    @Test
    @Feature({"TextInput"})
    public void testOrderConstants() {
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.FLOATING, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        int defaultItemsOffset = SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS - 1;
        assertEquals(defaultItemsOffset + DefaultItem.CUT, menuItems.get(0).order);
        assertEquals(defaultItemsOffset + DefaultItem.COPY, menuItems.get(1).order);
        assertEquals(defaultItemsOffset + DefaultItem.PASTE, menuItems.get(2).order);
        assertEquals(defaultItemsOffset + DefaultItem.PASTE_AS_PLAIN_TEXT, menuItems.get(3).order);
        assertEquals(defaultItemsOffset + DefaultItem.SHARE, menuItems.get(4).order);
        assertEquals(defaultItemsOffset + DefaultItem.SELECT_ALL, menuItems.get(5).order);
        assertEquals(defaultItemsOffset + DefaultItem.WEB_SEARCH, menuItems.get(6).order);
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrder_editable_cannotSelectAll() {
        when(mDelegate.canSelectAll(MenuType.DROPDOWN)).thenReturn(false);
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);

        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.DROPDOWN, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(6, menuItems.size());
        assertIdsAreEqual(R.id.select_action_menu_cut, menuItems.get(0).id);
        assertIdsAreEqual(R.id.select_action_menu_copy, menuItems.get(1).id);
        assertIdsAreEqual(android.R.id.paste, menuItems.get(2).id);
        assertIdsAreEqual(android.R.id.pasteAsPlainText, menuItems.get(3).id);
        assertIdsAreEqual(R.id.select_action_menu_share, menuItems.get(4).id);
        assertIdsAreEqual(R.id.select_action_menu_web_search, menuItems.get(5).id);
    }

    @Test
    @Feature({"TextInput"})
    public void testDisabledMenuItems_dropdown_areNotPresent() {
        setCapabilities(Set.of()); // Disable all capabilities

        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.DROPDOWN, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(0, menuItems.size());
    }

    @Test
    @Feature({"TextInput"})
    public void testDisabledMenuItems_floating_areNotPresent() {
        setCapabilities(Set.of()); // Disable all capabilities

        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addDefaultItems(
                pendingMenu, mContext, mDelegate, MenuType.FLOATING, mBuilder, null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(0, menuItems.size());
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrderUsingSelectionActionMenuDelegate_dropdown() {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);

        SelectActionMenuHelper.addDefaultItems(
                pendingMenu,
                mContext,
                mDelegate,
                MenuType.DROPDOWN,
                mBuilder,
                selectionActionMenuDelegate);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertIdsAreEqual(R.id.select_action_menu_cut, menuItems.get(0).id);
        assertIdsAreEqual(R.id.select_action_menu_copy, menuItems.get(1).id);
        assertIdsAreEqual(android.R.id.paste, menuItems.get(2).id);
        assertIdsAreEqual(android.R.id.pasteAsPlainText, menuItems.get(3).id);
        assertIdsAreEqual(R.id.select_action_menu_select_all, menuItems.get(4).id);
        assertIdsAreEqual(R.id.select_action_menu_share, menuItems.get(5).id);
        assertIdsAreEqual(R.id.select_action_menu_web_search, menuItems.get(6).id);
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrderUsingSelectionActionMenuDelegate_floating() {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);

        SelectActionMenuHelper.addDefaultItems(
                pendingMenu,
                mContext,
                mDelegate,
                MenuType.FLOATING,
                mBuilder,
                selectionActionMenuDelegate);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertIdsAreEqual(R.id.select_action_menu_cut, menuItems.get(0).id);
        assertIdsAreEqual(R.id.select_action_menu_copy, menuItems.get(1).id);
        assertIdsAreEqual(android.R.id.paste, menuItems.get(2).id);
        assertIdsAreEqual(android.R.id.pasteAsPlainText, menuItems.get(3).id);
        assertIdsAreEqual(R.id.select_action_menu_select_all, menuItems.get(4).id);
        assertIdsAreEqual(R.id.select_action_menu_share, menuItems.get(5).id);
        assertIdsAreEqual(R.id.select_action_menu_web_search, menuItems.get(6).id);
    }

    @Test
    @Feature({"TextInput"})
    public void testAddTextProcessingItems() {
        ContextUtils.initApplicationContextForTests(mContext);
        List<ResolveInfo> list2 = new ArrayList<>();
        ResolveInfo resolveInfo2 = createResolveInfoWithActivityInfo("ProcessTextActivity2", true);
        list2.add(resolveInfo2);
        doReturn(mPackageManager).when(mContext).getPackageManager();
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list2);
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addTextProcessingItems(
                pendingMenu,
                mContext,
                MenuType.FLOATING,
                false,
                true,
                "test",
                true,
                mBuilder,
                null);
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertNotNull(menuItems);
        assertEquals(1, menuItems.size());

        pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addTextProcessingItems(
                pendingMenu,
                mContext,
                MenuType.FLOATING,
                false,
                true,
                "test",
                true,
                mBuilder,
                selectionActionMenuDelegate);
        menuItems = pendingMenu.getMenuItemsForTesting();
        assertNotNull(menuItems);
        assertTrue(menuItems.isEmpty());
    }

    @Test
    @Feature({"TextInput"})
    public void testAddTextProcessingItems_emptySelection() {
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        SelectActionMenuHelper.addTextProcessingItems(
                pendingMenu, mContext, MenuType.FLOATING, false, true, "", true, mBuilder, null);
        assertTrue(pendingMenu.getMenuItemsForTesting().isEmpty());
    }

    @Test
    @Feature({"TextInput"})
    public void setMenuItemOrder_doesNotSpillIntoNextGroup() {
        int largeOffset = 10000;
        SelectionMenuItem primaryAssistItem =
                new SelectionMenuItem.Builder("")
                        .setOrderAndCategory(
                                largeOffset, SelectionMenuItem.ItemGroupOffset.ASSIST_ITEMS)
                        .build();
        SelectionMenuItem defaultItem =
                new SelectionMenuItem.Builder("")
                        .setOrderAndCategory(
                                largeOffset, SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS)
                        .build();
        SelectionMenuItem secondaryAssistItem =
                new SelectionMenuItem.Builder("")
                        .setOrderAndCategory(
                                largeOffset,
                                SelectionMenuItem.ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                        .build();

        assertTrue(primaryAssistItem.order < SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS);
        assertTrue(defaultItem.order < SelectionMenuItem.ItemGroupOffset.SECONDARY_ASSIST_ITEMS);
        assertTrue(
                secondaryAssistItem.order
                        < SelectionMenuItem.ItemGroupOffset.TEXT_PROCESSING_ITEMS);
    }

    private ResolveInfo createResolveInfoWithActivityInfo(String activityName, boolean exported) {
        String packageName = "org.chromium.content.browser.selection.SelectActionMenuHelperTest";

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
                        return "TEST_LABEL";
                    }
                };
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private void setCapabilities(Set<@DefaultItem Integer> capabilities) {
        when(mDelegate.canCut()).thenReturn(capabilities.contains(DefaultItem.CUT));
        when(mDelegate.canCopy()).thenReturn(capabilities.contains(DefaultItem.COPY));
        when(mDelegate.canPaste()).thenReturn(capabilities.contains(DefaultItem.PASTE));
        when(mDelegate.canSelectAll(anyInt()))
                .thenReturn(capabilities.contains(DefaultItem.SELECT_ALL));
        when(mDelegate.canWebSearch(anyInt()))
                .thenReturn(capabilities.contains(DefaultItem.WEB_SEARCH));
        when(mDelegate.canPasteAsPlainText())
                .thenReturn(capabilities.contains(DefaultItem.PASTE_AS_PLAIN_TEXT));
        when(mDelegate.canShare(anyInt())).thenReturn(capabilities.contains(DefaultItem.SHARE));
    }

    /** Helper method for asserting IDs with more useful error messages. */
    private void assertIdsAreEqual(int expectedId, int actualId) {
        assertEquals(
                String.format(
                        Locale.ENGLISH,
                        "Expected %s but was actually %s",
                        ID_MAPPING.getOrDefault(expectedId, String.valueOf(expectedId)),
                        ID_MAPPING.getOrDefault(actualId, String.valueOf(actualId))),
                expectedId,
                actualId);
    }
}
