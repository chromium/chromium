// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class VideoCaptureFormat {
    // A Non-exhaustive list mirroring the ImageFormat.Format IntDef. Necessary to avoid suppressing
    // related lint errors, see https://crbug.com/1410903 for context.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
            value = {
                ImageFormat.UNKNOWN,
                PixelFormat.RGBA_8888,
                PixelFormat.RGBX_8888,
                PixelFormat.RGB_888,
                ImageFormat.RGB_565,
                ImageFormat.YV12,
                ImageFormat.NV16,
                ImageFormat.NV21,
                ImageFormat.YUY2,
                ImageFormat.JPEG,
                ImageFormat.YUV_420_888,
                ImageFormat.YUV_422_888,
                ImageFormat.YUV_444_888,
                ImageFormat.FLEX_RGB_888,
                ImageFormat.FLEX_RGBA_8888,
                ImageFormat.RAW_SENSOR,
                ImageFormat.RAW_PRIVATE,
                ImageFormat.RAW10,
                ImageFormat.RAW12,
                ImageFormat.DEPTH16,
                ImageFormat.DEPTH_POINT_CLOUD,
                ImageFormat.PRIVATE
            })
    public @interface Format {}

    int mWidth;
    int mHeight;
    final int mFramerate;
    @Format final int mPixelFormat;

    public VideoCaptureFormat(int width, int height, int framerate, int pixelformat) {
        mWidth = width;
        mHeight = height;
        mFramerate = framerate;
        mPixelFormat = pixelformat;
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }

    public int getFramerate() {
        return mFramerate;
    }

    public @Format int getPixelFormat() {
        return mPixelFormat;
    }
}
