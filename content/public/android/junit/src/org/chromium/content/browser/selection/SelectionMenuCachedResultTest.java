// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.view.textclassifier.TextClassification;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionClient.Result;
import org.chromium.content_public.browser.SelectionMenuGroup;

import java.util.SortedSet;
import java.util.TreeSet;

/** Unit tests for {@link SelectionMenuCachedResult}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionMenuCachedResultTest {
    @Mock private TextClassification mTextClassification1;
    @Mock private TextClassification mTextClassification2;
    private final SelectionClient.Result mClassificationResult1 = new Result();
    private final SelectionClient.Result mClassificationResult2 = new Result();

    private SortedSet<SelectionMenuGroup> mMenuItems;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMenuItems = new TreeSet<>();

        mClassificationResult1.setTextClassificationForTesting(mTextClassification1);
        Mockito.when(mTextClassification1.getText()).thenReturn("phone");
    }

    @Test
    public void testCachedMenuResultGetter() {
        SelectionMenuGroup defaultGroup = new SelectionMenuGroup(1, 1);
        mMenuItems.add(defaultGroup);

        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        assertEquals(menuParams.getResult(), mMenuItems);
    }

    @Test
    public void testCanBeReusedForDifferentIsSelectionPassword() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(null, true, true, "test"));
    }

    @Test
    public void testCanBeReusedForDifferentIsSelectionReadOnly() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(null, false, false, "test"));
    }

    @Test
    public void testCanBeReusedForDifferentSelectedText() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(null, false, true, "test2"));
    }

    @Test
    public void testCanBeReusedForNullAndNonNullClassificationResult() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(null, false, true, "test"));
    }

    @Test
    public void testCanBeReusedForDifferentClassificationResultText() {
        Mockito.when(mTextClassification2.getText()).thenReturn("map");
        mClassificationResult2.setTextClassificationForTesting(mTextClassification2);

        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(mClassificationResult2, false, true, "test"));
    }

    @Test
    public void testCanBeReusedForBothNullClassificationResultAndSimilarOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        Assert.assertTrue(menuParams.canReuseResult(null, false, true, "test"));
    }

    @Test
    public void testCanBeReusedForBothNullClassificationResultAndDifferentOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(null, false, true, "test", mMenuItems);

        assertFalse(menuParams.canReuseResult(null, true, false, "test"));
    }

    @Test
    public void testCanBeReusedForSimilarClassificationResultAndSimilarOtherParams() {
        SelectionMenuCachedResult menuParams =
                new SelectionMenuCachedResult(
                        mClassificationResult1, false, true, "test", mMenuItems);

        Assert.assertTrue(menuParams.canReuseResult(mClassificationResult1, false, true, "test"));
    }
}
