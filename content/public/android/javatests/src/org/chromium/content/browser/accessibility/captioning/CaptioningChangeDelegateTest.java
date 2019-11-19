// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.graphics.Color;
import android.graphics.Typeface;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/**
  * Test suite to ensure that platform settings are translated to CSS appropriately
  */
@RunWith(BaseJUnit4ClassRunner.class)
public class CaptioningChangeDelegateTest {
    private static final String DEFAULT_CAPTIONING_PREF_VALUE =
            CaptioningChangeDelegate.DEFAULT_CAPTIONING_PREF_VALUE;

    @Test
    @SmallTest
    public void testFontScaleToPercentage() {
        String result = CaptioningChangeDelegate.androidFontScaleToPercentage(0f);
        Assert.assertEquals("0%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.000f);
        Assert.assertEquals("0%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.25f);
        Assert.assertEquals("25%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(1f);
        Assert.assertEquals("100%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(1.5f);
        Assert.assertEquals("150%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.50125f);
        Assert.assertEquals("50%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.50925f);
        Assert.assertEquals("51%", result);
    }

    @Test
    @SmallTest
    public void testAndroidColorToCssColor() {
        String result = CaptioningChangeDelegate.androidColorToCssColor(null);
        Assert.assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.BLACK);
        Assert.assertEquals("rgba(0, 0, 0, 1)", result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.WHITE);
        Assert.assertEquals("rgba(255, 255, 255, 1)", result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.BLUE);
        Assert.assertEquals("rgba(0, 0, 255, 1)", result);

        // Transparent-black
        result = CaptioningChangeDelegate.androidColorToCssColor(0x00000000);
        Assert.assertEquals("rgba(0, 0, 0, 0)", result);

        // Transparent-white
        result = CaptioningChangeDelegate.androidColorToCssColor(0x00FFFFFF);
        Assert.assertEquals("rgba(255, 255, 255, 0)", result);

        // 50% opaque blue
        result = CaptioningChangeDelegate.androidColorToCssColor(0x7f0000ff);
        Assert.assertEquals("rgba(0, 0, 255, 0.5)", result);

        // No alpha information
        result = CaptioningChangeDelegate.androidColorToCssColor(0xFFFFFF);
        Assert.assertEquals("rgba(255, 255, 255, 0)", result);
    }

    @Test
    @SmallTest
    public void testClosedCaptionEdgeAttributeWithDefaults() {
        Assert.assertEquals(DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge(null, null));
        Assert.assertEquals(DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge("red", null));
        Assert.assertEquals(DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge("red", 0));
        Assert.assertEquals("silver 0.05em 0.05em 0.1em",
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge(null, 2));
        Assert.assertEquals("silver 0.05em 0.05em 0.1em",
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge("", 2));
        Assert.assertEquals("red 0.05em 0.05em 0.1em",
                CaptioningChangeDelegate.getShadowFromColorAndSystemEdge("red", 2));
    }

    /**
     * Verifies that certain system fonts always correspond to the default captioning font.
     */
    @Test
    @SmallTest
    public void testClosedCaptionDefaultFonts() {
        Assert.assertEquals("Null typeface should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getFontFromSystemFont(null));

        Assert.assertEquals("Typeface.DEFAULT should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getFontFromSystemFont(Typeface.DEFAULT));

        Assert.assertEquals("Typeface.BOLD should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE,
                CaptioningChangeDelegate.getFontFromSystemFont(Typeface.DEFAULT_BOLD));
    }

    /**
     * Typeface.DEFAULT may be equivalent to another Typeface such as Typeface.SANS_SERIF
     * so this test ensures that each typeface returns DEFAULT_CAPTIONING_PREF_VALUE if it is
     * equal to Typeface.DEFAULT or returns an explicit font family otherwise.
     */
    @Test
    @SmallTest
    public void testClosedCaptionNonDefaultFonts() {
        if (Typeface.MONOSPACE.equals(Typeface.DEFAULT)) {
            Assert.assertEquals(
                    "Since the default font is monospace, the default family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE,
                    CaptioningChangeDelegate.getFontFromSystemFont(Typeface.MONOSPACE));
        }

        if (Typeface.SANS_SERIF.equals(Typeface.DEFAULT)) {
            Assert.assertEquals(
                    "Since the default font is sans-serif, the default family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE,
                    CaptioningChangeDelegate.getFontFromSystemFont(Typeface.SANS_SERIF));
        }

        if (Typeface.SERIF.equals(Typeface.DEFAULT)) {
            Assert.assertEquals(
                    "Since the default font is serif, the default font family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE,
                    CaptioningChangeDelegate.getFontFromSystemFont(Typeface.SANS_SERIF));
        }
    }
}
