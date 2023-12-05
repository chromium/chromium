// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.content.R;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.common.ContentFeatures;

import java.util.List;

/** Unit tests for {@link SelectActionMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectActionMenuHelperTest {
    @Mock private SelectActionMenuHelper.SelectActionMenuDelegate mDelegate;
    @Rule public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();
    @Mock private Context mContext;

    private static class TestSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
        @Override
        public void getModifiedMenuItems(List<SelectionMenuItem.Builder> menuItemBuilders) {
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
        SelectionMenuGroup menuGroup = SelectActionMenuHelper.getDefaultItems(mContext, mDelegate,
                null);
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
                SelectActionMenuHelper.getDefaultItems(mContext, mDelegate,
                        selectionActionMenuDelegate);
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
}
