// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;

import com.google.android.gms.vision.Frame;

import org.chromium.mojo_base.BigBufferUtil;

import java.nio.ByteBuffer;

/** Utility class to convert a Bitmap to a GMS core YUV Frame. */
public class BitmapUtils {
    public static Bitmap convertToBitmap(org.chromium.skia.mojom.BitmapN32 bitmapData) {
        // A null BitmapN32 has null pixelData. Otherwise, BitmapN32 always has
        // a valid N32 (aka RGBA_8888 or BGRA_8888 depending on the build
        // config) colorType.
        if (bitmapData.pixelData == null) {
            return null;
        }

        int width = bitmapData.imageInfo.width;
        int height = bitmapData.imageInfo.height;
        final long numPixels = (long) width * height;
        // TODO(mcasas): https://crbug.com/670028 homogeneize overflow checking.
        if (width <= 0 || height <= 0 || numPixels > (Long.MAX_VALUE / 4)) {
            return null;
        }

        try (BigBufferUtil.Mapping mapping = BigBufferUtil.map(bitmapData.pixelData)) {
            ByteBuffer imageBuffer = mapping.getBuffer();
            if (imageBuffer.capacity() <= 0) {
                return null;
            }

            Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            bitmap.copyPixelsFromBuffer(imageBuffer);
            return bitmap;
        }
    }

    public static Frame convertToFrame(org.chromium.skia.mojom.BitmapN32 bitmapData) {
        Bitmap bitmap = convertToBitmap(bitmapData);
        if (bitmap == null) {
            return null;
        }

        // This constructor implies a pixel format conversion to YUV.
        return new Frame.Builder().setBitmap(bitmap).build();
    }
}
