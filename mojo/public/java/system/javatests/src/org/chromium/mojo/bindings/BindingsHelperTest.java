// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.nio.charset.Charset;

/** Testing {@link BindingsHelper}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BindingsHelperTest {
    /** Testing {@link BindingsHelper#utf8StringSizeInBytes(String)}. */
    @Test
    @SmallTest
    public void testUTF8StringLength() {
        String[] stringsToTest = {
            "", "a", "hello world", "éléphant", "𠜎𠜱𠝹𠱓𠱸𠲖𠳏𠳕", "你午饭想吃什么", "你午饭想吃什么\0éléphant",
        };
        for (String s : stringsToTest) {
            Assert.assertEquals(
                    s.getBytes(Charset.forName("utf8")).length,
                    BindingsHelper.utf8StringSizeInBytes(s));
        }
        Assert.assertEquals(1, BindingsHelper.utf8StringSizeInBytes("\0"));
        String s =
                new StringBuilder()
                        .appendCodePoint(0x0)
                        .appendCodePoint(0x80)
                        .appendCodePoint(0x800)
                        .appendCodePoint(0x10000)
                        .toString();
        Assert.assertEquals(10, BindingsHelper.utf8StringSizeInBytes(s));
        Assert.assertEquals(10, s.getBytes(Charset.forName("utf8")).length);
    }

    /** Testing {@link BindingsHelper#align(int)}. */
    @Test
    @SmallTest
    public void testAlign() {
        for (int i = 0; i < 3 * BindingsHelper.ALIGNMENT; ++i) {
            int j = BindingsHelper.align(i);
            Assert.assertTrue(j >= i);
            Assert.assertTrue(j % BindingsHelper.ALIGNMENT == 0);
            Assert.assertTrue(j - i < BindingsHelper.ALIGNMENT);
        }
    }
}
