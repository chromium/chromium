// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

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

/** Unit tests for {@link SelectActionMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectActionMenuHelperTest {
    @Mock private SelectActionMenuHelper.TextSelectionCapabilitiesDelegate mDelegate;
    @Mock private Context mContext;

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
        MockitoAnnotations.initMocks(this);
        // Used to mock out getting menu item icons.
        TypedArray a = mock(TypedArray.class);
        when(mContext.obtainStyledAttributes(any(int[].class))).thenReturn(a);
        when(a.getResourceId(anyInt(), anyInt())).thenReturn(0);

        when(mDelegate.canCut()).thenReturn(true);
        when(mDelegate.canCopy()).thenReturn(true);
        when(mDelegate.canPaste()).thenReturn(true);
        when(mDelegate.canSelectAll()).thenReturn(true);
        when(mDelegate.canWebSearch()).thenReturn(true);
        when(mDelegate.canPasteAsPlainText()).thenReturn(true);
        when(mDelegate.canShare()).thenReturn(true);
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrder() {
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        pendingMenu.addAll(
                SelectActionMenuHelper.getDefaultItems(
                        mContext, mDelegate, MenuType.FLOATING, null));
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertEquals(menuItems.get(0).id, R.id.select_action_menu_cut);
        assertEquals(menuItems.get(1).id, R.id.select_action_menu_copy);
        assertEquals(menuItems.get(2).id, R.id.select_action_menu_paste);
        assertEquals(menuItems.get(3).id, R.id.select_action_menu_paste_as_plain_text);
        assertEquals(menuItems.get(4).id, R.id.select_action_menu_share);
        assertEquals(menuItems.get(5).id, R.id.select_action_menu_select_all);
        assertEquals(menuItems.get(6).id, R.id.select_action_menu_web_search);
    }

    @Test
    @Feature({"TextInput"})
    public void testDefaultMenuItemsOrderUsingSelectionActionMenuDelegate() {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        pendingMenu.addAll(
                SelectActionMenuHelper.getDefaultItems(
                        mContext, mDelegate, MenuType.FLOATING, selectionActionMenuDelegate));
        List<SelectionMenuItem> menuItems = pendingMenu.getMenuItemsForTesting();
        assertEquals(7, menuItems.size());
        assertEquals(menuItems.get(0).id, R.id.select_action_menu_cut);
        assertEquals(menuItems.get(1).id, R.id.select_action_menu_copy);
        assertEquals(menuItems.get(2).id, R.id.select_action_menu_paste);
        assertEquals(menuItems.get(3).id, R.id.select_action_menu_paste_as_plain_text);
        assertEquals(menuItems.get(4).id, R.id.select_action_menu_select_all);
        assertEquals(menuItems.get(5).id, R.id.select_action_menu_share);
        assertEquals(menuItems.get(6).id, R.id.select_action_menu_web_search);
    }

    @Test
    @Feature({"TextInput"})
    public void testGetTextProcessingItems() {
        ContextUtils.initApplicationContextForTests(mContext);
        List<ResolveInfo> list2 = new ArrayList();
        ResolveInfo resolveInfo2 = createResolveInfoWithActivityInfo("ProcessTextActivity2", true);
        list2.add(resolveInfo2);
        PackageManager pm = mock(PackageManager.class);
        doReturn(pm).when(mContext).getPackageManager();
        when(pm.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list2);
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        List<SelectionMenuItem> textProcessingItems =
                SelectActionMenuHelper.getTextProcessingItems(
                        mContext, MenuType.FLOATING, false, true, "test", true, null);
        assertNotNull(textProcessingItems);
        assertEquals(1, textProcessingItems.size());

        textProcessingItems =
                SelectActionMenuHelper.getTextProcessingItems(
                        mContext,
                        MenuType.FLOATING,
                        false,
                        true,
                        "test",
                        true,
                        selectionActionMenuDelegate);
        assertNotNull(textProcessingItems);
        assertTrue(textProcessingItems.isEmpty());
    }

    @Test
    @Feature({"TextInput"})
    public void testGetTextProcessingItems_emptySelection() {
        List<SelectionMenuItem> textProcessingItems =
                SelectActionMenuHelper.getTextProcessingItems(
                        mContext, MenuType.FLOATING, false, true, "", true, null);
        assertNull(textProcessingItems);
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
}
