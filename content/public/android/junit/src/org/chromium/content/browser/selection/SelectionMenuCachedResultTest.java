// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.anyInt;

import android.content.Context;
import android.view.textclassifier.TextClassification;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.PendingSelectionMenu;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionClient.Result;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.ui.base.Clipboard;

/** Unit tests for {@link SelectionMenuCachedResult}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionMenuCachedResultTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TextClassification mTextClassification1;
    @Mock private TextClassification mTextClassification2;
    @Mock private SelectionActionMenuDelegate mSelectionActionMenuDelegate;
    @Mock private Context mContext;
    private final SelectionClient.Result mClassificationResult1 = new Result();
    private final SelectionClient.Result mClassificationResult2 = new Result();

    private PendingSelectionMenu mMenuItems;

    @Before
    public void setUp() {
        mMenuItems = new PendingSelectionMenu(mContext);

        mClassificationResult1.setTextClassificationForTesting(mTextClassification1);
        Mockito.when(mSelectionActionMenuDelegate.canReuseCachedSelectionMenu(anyInt()))
                .thenReturn(true);
        Mockito.when(mTextClassification1.getText()).thenReturn("phone");
    }

    @Test
    public void testCachedMenuResultGetter() {
        mMenuItems.addMenuItem(
                new SelectionMenuItem.Builder("Test")
                        .setOrderAndCategory(0, SelectionMenuItem.ItemGroupOffset.DEFAULT_ITEMS)
                        .build());

        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertEquals(menuParams.getResult(), mMenuItems);
    }

    @Test
    public void testCanBeReusedForDifferentIsSelectionPassword() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null, true, true, "test", MenuType.FLOATING, mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForDifferentIsSelectionReadOnly() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null,
                        false,
                        false,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForDifferentSelectedText() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null,
                        false,
                        true,
                        "test2",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanNotBeReusedForDifferentMenuTypes() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null,
                        false,
                        true,
                        "test2",
                        MenuType.DROPDOWN,
                        mSelectionActionMenuDelegate));

        SelectionMenuCachedResult menuParams2 =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.DROPDOWN, mMenuItems);

        assertFalse(
                menuParams2.canReuseResult(
                        null,
                        false,
                        true,
                        "test2",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForNullAndNonNullClassificationResult() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForDifferentClassificationResultText() {
        Mockito.when(mTextClassification2.getText()).thenReturn("map");
        mClassificationResult2.setTextClassificationForTesting(mTextClassification2);

        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        mClassificationResult2,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForBothNullClassificationResultAndSimilarOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        Assert.assertTrue(
                menuParams.canReuseResult(
                        null,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForBothNullClassificationResultAndDifferentOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 0, MenuType.FLOATING, mMenuItems);

        assertFalse(
                menuParams.canReuseResult(
                        null,
                        true,
                        false,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testCanBeReusedForSimilarClassificationResultAndSimilarOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);

        Assert.assertTrue(
                menuParams.canReuseResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void
            testCanBeReusedForSimilarClassificationResultAndParamsIfCachingNotAllowedByDelegate() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);
        Mockito.when(mSelectionActionMenuDelegate.canReuseCachedSelectionMenu(anyInt()))
                .thenReturn(false);

        Assert.assertFalse(
                menuParams.canReuseResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }

    @Test
    public void testIsSameSelection() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        null, false, true, "test", 10, MenuType.FLOATING, mMenuItems);

        // Same params
        Assert.assertTrue(menuParams.isSameSelection("test", 10, false, true, MenuType.FLOATING));

        // Different text
        Assert.assertFalse(
                menuParams.isSameSelection("different", 10, false, true, MenuType.FLOATING));

        // Different offset
        Assert.assertFalse(menuParams.isSameSelection("test", 20, false, true, MenuType.FLOATING));

        // Different password
        Assert.assertFalse(menuParams.isSameSelection("test", 10, true, true, MenuType.FLOATING));

        // Different read only
        Assert.assertFalse(menuParams.isSameSelection("test", 10, false, false, MenuType.FLOATING));

        // Different menu type
        Assert.assertFalse(menuParams.isSameSelection("test", 10, false, true, MenuType.DROPDOWN));
    }

    @Test
    public void clipboardChangePreventsCacheReuse() {
        Clipboard testClipboard =
                new Clipboard() {
                    private String mText = "";

                    @Override
                    public boolean canPaste() {
                        return !"".equals(mText);
                    }

                    @Override
                    public void setText(String text) {
                        mText = text;
                    }
                };
        Clipboard.setInstanceForTesting(testClipboard);
        // Create params with empty clipboard.
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);
        testClipboard.setText("Some text");
        // Check params with non-empty clipboard.
        Assert.assertFalse(
                menuParams.canReuseResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));

        // Create params with non-empty clipboard.
        SelectionMenuCachedResult menuParams2 =
                new SelectionMenuCachedResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        0,
                        MenuType.FLOATING,
                        mMenuItems);
        testClipboard.setText("");
        // Check params with empty clipboard.
        Assert.assertFalse(
                menuParams2.canReuseResult(
                        mClassificationResult1,
                        false,
                        true,
                        "test",
                        MenuType.FLOATING,
                        mSelectionActionMenuDelegate));
    }
}
