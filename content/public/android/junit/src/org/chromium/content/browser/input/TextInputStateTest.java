// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Unit tests for {@TextInputState}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class TextInputStateTest {
    @Test
    @Feature({"TextInput"})
    public void testEmptySelection() {
        TextInputState state =
                new TextInputState("hello", new Range(3, 3), new Range(-1, -1), false, true);
        assertEquals("lo", state.getTextAfterSelection(Integer.MAX_VALUE));
        assertEquals("lo", state.getTextAfterSelection(3));
        assertEquals("lo", state.getTextAfterSelection(2));
        assertEquals("", state.getTextAfterSelection(0));
        assertEquals("", state.getTextAfterSelection(-1));
        assertEquals("hel", state.getTextBeforeSelection(Integer.MAX_VALUE));
        assertEquals("hel", state.getTextBeforeSelection(3));
        assertEquals("el", state.getTextBeforeSelection(2));
        assertEquals("", state.getTextBeforeSelection(0));
        assertEquals("", state.getTextBeforeSelection(-1));
        assertEquals(null, state.getSelectedText());
    }

    @Test
    @Feature({"TextInput"})
    public void testNonEmptySelection() {
        TextInputState state =
                new TextInputState("hello", new Range(3, 4), new Range(3, 4), false, true);
        assertEquals("hel", state.getTextBeforeSelection(Integer.MAX_VALUE));
        assertEquals("hel", state.getTextBeforeSelection(4));
        assertEquals("hel", state.getTextBeforeSelection(3));
        assertEquals("", state.getTextBeforeSelection(0));
        assertEquals("", state.getTextBeforeSelection(-1));
        assertEquals("o", state.getTextAfterSelection(Integer.MAX_VALUE));
        assertEquals("o", state.getTextAfterSelection(2));
        assertEquals("o", state.getTextAfterSelection(1));
        assertEquals("", state.getTextAfterSelection(0));
        assertEquals("", state.getTextAfterSelection(-1));
        assertEquals("l", state.getSelectedText());
    }
}
