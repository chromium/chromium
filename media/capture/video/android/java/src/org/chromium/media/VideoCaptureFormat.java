// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

class VideoCaptureFormat {
    int mWidth;
    int mHeight;
    final int mFramerate;
    final int mPixelFormat;

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

    public int getPixelFormat() {
        return mPixelFormat;
    }
}
