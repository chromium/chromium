// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.content.R;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.common.ContentFeatures;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SelectActionMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectActionMenuHelperTest {
    @Mock private SelectActionMenuHelper.SelectActionMenuDelegate mDelegate;
    @Mock private Context mContext;

    private static class TestSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
        @Override
        public void modifyDefaultMenuItems(
                List<SelectionMenuItem.Builder> menuItemBuilders,
                boolean isSelectionPassword,
                String selectedText) {
            for (SelectionMenuItem.Builder builder : menuItemBuilders) {
                int menuItemOrder = getMenuItemOrder(builder.mId);
                if (menuItemOrder == -1) continue;
                builder.setOrderInCategory(menuItemOrder);
            }
        }

        private int getMenuItemOrder(int id) {
            if (id == R.id.select_action_menu_cut) {
                return 1;
            } else if (id == R.id.select_action_menu_copy) {
                return 2;
            } else if (id == R.id.select_action_menu_paste) {
                return 3;
            } else if (id == R.id.select_action_menu_paste_as_plain_text) {
                return 4;
            } else if (id == R.id.select_action_menu_select_all) {
                return 5;
            } else if (id == R.id.select_action_menu_share) {
                return 6;
            } else if (id == R.id.select_action_menu_web_search) {
                return 7;
            }
            return -1;
        }

        @Override
        public List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities) {
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
        public List<SelectionMenuItem> getAdditionalNonSelectionItems() {
            return new ArrayList<>();
        }

        @Override
        public List<SelectionMenuItem> getAdditionalTextProcessingItems() {
            return new ArrayList<>();
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
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
    @Features.EnableFeatures({ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION})
    public void testDefaultMenuItemsOrder() {
        SelectionMenuGroup menuGroup =
                SelectActionMenuHelper.getDefaultItems(
                        mContext,
                        mDelegate,
                        null,
                        /* isSelectionPassword= */ true,
                        /* selectedText= */ "test");
        assertEquals(7, menuGroup.items.size());
        SelectionMenuItem[] items = menuGroup.items.toArray(new SelectionMenuItem[0]);
        assertEquals(items[0].id, R.id.select_action_menu_cut);
        assertEquals(items[1].id, R.id.select_action_menu_copy);
        assertEquals(items[2].id, R.id.select_action_menu_paste);
        assertEquals(items[3].id, R.id.select_action_menu_paste_as_plain_text);
        assertEquals(items[4].id, R.id.select_action_menu_share);
        assertEquals(items[5].id, R.id.select_action_menu_select_all);
        assertEquals(items[6].id, R.id.select_action_menu_web_search);
    }

    @Test
    @Feature({"TextInput"})
    @Features.EnableFeatures({ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION})
    public void testDefaultMenuItemsOrderUsingSelectionActionMenuDelegate() {
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        SelectionMenuGroup menuGroup =
                SelectActionMenuHelper.getDefaultItems(
                        mContext,
                        mDelegate,
                        selectionActionMenuDelegate,
                        /* isSelectionPassword= */ true,
                        /* selectedText= */ "test");
        assertEquals(7, menuGroup.items.size());
        SelectionMenuItem[] items = menuGroup.items.toArray(new SelectionMenuItem[0]);
        assertEquals(items[0].id, R.id.select_action_menu_cut);
        assertEquals(items[1].id, R.id.select_action_menu_copy);
        assertEquals(items[2].id, R.id.select_action_menu_paste);
        assertEquals(items[3].id, R.id.select_action_menu_paste_as_plain_text);
        assertEquals(items[4].id, R.id.select_action_menu_select_all);
        assertEquals(items[5].id, R.id.select_action_menu_share);
        assertEquals(items[6].id, R.id.select_action_menu_web_search);
    }

    @Test
    @Feature({"TextInput"})
    @Features.EnableFeatures({ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION})
    public void testGetTextProcessingItems() {
        ContextUtils.initApplicationContextForTests(mContext);
        List<ResolveInfo> list2 = new ArrayList();
        ResolveInfo resolveInfo2 = createResolveInfoWithActivityInfo("ProcessTextActivity2", true);
        list2.add(resolveInfo2);
        PackageManager pm = mock(PackageManager.class);
        doReturn(pm).when(mContext).getPackageManager();
        when(pm.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list2);
        SelectActionMenuHelper.TextProcessingIntentHandler intentHandler =
                new SelectActionMenuHelper.TextProcessingIntentHandler() {
                    @Override
                    public void handleIntent(Intent textProcessingIntent) {}
                };
        SelectionActionMenuDelegate selectionActionMenuDelegate =
                new TestSelectionActionMenuDelegate();
        SelectionMenuGroup group =
                SelectActionMenuHelper.getTextProcessingItems(
                        mContext, false, true, intentHandler, null);
        assertEquals(1, group.items.size());

        group =
                SelectActionMenuHelper.getTextProcessingItems(
                        mContext, false, true, intentHandler, selectionActionMenuDelegate);
        assertEquals(0, group.items.size());
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
