// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Size;
import android.view.Display;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.media.MediaCodecUtil.MimeTypes;

/**
 * A utility class to make an estimate for the hints provided to MediaFormat as to the expected
 * maximum resolution to prepare for.
 */
public class MaxAnticipatedResolutionEstimator {
    private static final String TAG = "EstimateResolution";
    private static final int SCREEN_WIDTH_4K = 3840;
    private static final int SCREEN_HEIGHT_4K = 2160;

    /** Class to represent display resolution. */
    public static class Resolution {
        int mWidth;
        int mHeight;

        public Resolution(int width, int height) {
            mWidth = width;
            mHeight = height;
        }

        public int getWidth() {
            return mWidth;
        }

        public int getHeight() {
            return mHeight;
        }
    }

    private MaxAnticipatedResolutionEstimator() {}

    public static Resolution getScreenResolution(MediaFormat format) {
        Resolution resolution = getNativeResolution();
        if (resolution == null) {
            resolution.mWidth = format.getInteger(MediaFormat.KEY_WIDTH);
            resolution.mHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
        }

        // Cap screen size at 1080p for non-4K codecs
        String mimeType = format.getString(MediaFormat.KEY_MIME);
        if (!mimeType.equals(MimeTypes.VIDEO_HEVC)
                && !mimeType.equals(MimeTypes.VIDEO_VP9)
                && !mimeType.equals(MimeTypes.VIDEO_AV1)
                && !mimeType.equals(MimeTypes.VIDEO_DV)) {
            resolution.mWidth = Math.min(resolution.mWidth, 1920);
            resolution.mHeight = Math.min(resolution.mHeight, 1080);
        }
        return resolution;
    }

    @Nullable
    public static Resolution getNativeResolution() {
        // Starting with P, DisplayCompat relies on having read access to
        // vendor.display-size (except for devices that correctly implement
        // DisplayMode#getPhysicalHeight / getPhysicalWidth
        // (e.g. Nvidia Shield).
        // Unfortunately, before Q, SoC vendors did not grant such access to
        // priv_app in their SELinux policy files. This means that for P devices
        // (except Nvidia Shield), we should continue to guess display size by
        // looking at the installed codecs.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.P
                && !isNvidiaShield()
                && is4kVpxSupported()) {
            Log.d(TAG, "Assuming 4K display capabilities because we can decode VP9 4K video.");
            return new Resolution(SCREEN_WIDTH_4K, SCREEN_HEIGHT_4K);
        }
        // If we can't establish 4k support from the codecs, it's best to
        // fall back on DisplayCompat.

        Context context = ContextUtils.getApplicationContext();
        DisplayManager displayManager =
                (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);

        DisplayCompat.ModeCompat[] supportedModes =
                DisplayCompat.getSupportedModes(
                        context, displayManager.getDisplay(Display.DEFAULT_DISPLAY));

        // supportedModes always contain at least one native mode.
        // All native modes are equal in resolution (but differ in refresh rates).
        for (DisplayCompat.ModeCompat mode : supportedModes) {
            if (mode.isNative()) {
                return new Resolution(mode.getPhysicalWidth(), mode.getPhysicalHeight());
            }
        }

        // Should never happen.
        return null;
    }

    private static boolean isNvidiaShield() {
        return "NVIDIA".equals(Build.MANUFACTURER) && Build.MODEL.startsWith("SHIELD");
    }

    private static boolean is4kVpxSupported() {
        return ScreenResolutionUtil.isResolutionSupportedForType(
                "video/x-vnd.on2.vp9", new Size(SCREEN_WIDTH_4K, SCREEN_HEIGHT_4K));
    }
}
