// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.mojo_base.BigBufferUtil;
import org.chromium.skia.mojom.BitmapN32;
import org.chromium.skia.mojom.BitmapN32ImageInfo;

/** Test suite for conversion-to-Frame utils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BitmapUtilsTest {
    private static final int VALID_WIDTH = 1;
    private static final int VALID_HEIGHT = 1;
    private static final int INVALID_WIDTH = 0;
    private static final long NUM_BYTES = VALID_WIDTH * VALID_HEIGHT * 4;
    private static final byte[] EMPTY_DATA = new byte[0];

    public BitmapUtilsTest() {}

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
    }

    /** Verify conversion fails if the Bitmap is invalid. */
    @Test
    @Feature({"ShapeDetection"})
    public void testConversionFailsWithInvalidBitmap() {
        BitmapN32 bitmap = new BitmapN32();
        bitmap.pixelData = null;
        bitmap.imageInfo = new BitmapN32ImageInfo();

        assertNull(BitmapUtils.convertToFrame(bitmap));
    }

    /** Verify conversion fails if the sent dimensions are ugly. */
    @Test
    @Feature({"ShapeDetection"})
    public void testConversionFailsWithInvalidDimensions() {
        BitmapN32 bitmap = new BitmapN32();
        bitmap.imageInfo = new BitmapN32ImageInfo();
        bitmap.pixelData = BigBufferUtil.createBigBufferFromBytes(EMPTY_DATA);
        bitmap.imageInfo.width = INVALID_WIDTH;
        bitmap.imageInfo.height = VALID_HEIGHT;

        assertNull(BitmapUtils.convertToFrame(bitmap));
    }

    /** Verify conversion fails if Bitmap fails to wrap(). */
    @Test
    @Feature({"ShapeDetection"})
    public void testConversionFailsWithWronglyWrappedData() {
        BitmapN32 bitmap = new BitmapN32();
        bitmap.imageInfo = new BitmapN32ImageInfo();
        bitmap.pixelData = BigBufferUtil.createBigBufferFromBytes(EMPTY_DATA);
        bitmap.imageInfo.width = VALID_WIDTH;
        bitmap.imageInfo.height = VALID_HEIGHT;

        assertNull(BitmapUtils.convertToFrame(bitmap));
    }
}
