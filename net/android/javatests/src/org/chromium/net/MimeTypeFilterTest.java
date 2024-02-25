// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.ArrayList;

/** Tests for MimeTypeFilter. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MimeTypeFilterTest {
    @Test
    @SmallTest
    public void testAcceptAllMimeTypes() {
        ArrayList<String> mimeTypes = new ArrayList<String>();
        mimeTypes.add("*/*");
        MimeTypeFilter mimeFilter = new MimeTypeFilter(mimeTypes, false);
        Assert.assertTrue(mimeFilter.accept(null, "image/jpeg"));
    }

    @Test
    @SmallTest
    public void testAcceptAllImageTypes() {
        ArrayList<String> mimeTypes = new ArrayList<String>();
        mimeTypes.add("image/*");
        MimeTypeFilter mimeFilter = new MimeTypeFilter(mimeTypes, false);
        Assert.assertTrue(mimeFilter.accept(null, "image/jpeg"));
        Assert.assertTrue(mimeFilter.accept(null, "image/gif"));
    }

    @Test
    @SmallTest
    public void testAcceptOnlyOneType() {
        ArrayList<String> mimeTypes = new ArrayList<String>();
        mimeTypes.add("text/plain");
        MimeTypeFilter mimeFilter = new MimeTypeFilter(mimeTypes, false);
        Assert.assertTrue(mimeFilter.accept(null, "text/plain"));
        Assert.assertFalse(mimeFilter.accept(null, "image/gif"));
    }

    @Test
    @SmallTest
    public void testAcceptExtension() {
        ArrayList<String> mimeTypes = new ArrayList<String>();
        mimeTypes.add(".jpeg");
        MimeTypeFilter mimeFilter = new MimeTypeFilter(mimeTypes, false);
        Uri jpegUri = Uri.parse("image.jpeg");
        Uri gifUri = Uri.parse("image.gif");
        Assert.assertTrue(mimeFilter.accept(jpegUri, ""));
        Assert.assertFalse(mimeFilter.accept(gifUri, ""));
    }
}
