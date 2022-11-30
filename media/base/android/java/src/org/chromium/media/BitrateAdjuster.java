// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class BitrateAdjuster {
    @IntDef({Type.NO_ADJUSTMENT, Type.FRAMERATE_ADJUSTMENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // No adjustment - video encoder has no known bitrate problem.
        int NO_ADJUSTMENT = 0;
        // Framerate based bitrate adjustment is required - HW encoder does not use frame
        // timestamps to calculate frame bitrate budget and instead is relying on initial
        // fps configuration assuming that all frames are coming at fixed initial frame rate.
        int FRAMERATE_ADJUSTMENT = 1;
    }

    private static final int FRAMERATE_ADJUSTMENT_BITRATE_ADJUSTMENT_FPS = 30;

    // Gets the adjusted bitrate according to the implementation's adjustment policy.
    public static int getTargetBitrate(@Type int type, int bps, int frameRate) {
        switch (type) {
            case Type.NO_ADJUSTMENT:
                return bps;
            case Type.FRAMERATE_ADJUSTMENT:
                return frameRate == 0
                        ? bps
                        : FRAMERATE_ADJUSTMENT_BITRATE_ADJUSTMENT_FPS * bps / frameRate;
        }
        return 0;
    }

    // Gets the initial frame rate of the media. The frameRateHint can be used as a default or a
    // constraint.
    public static int getInitialFrameRate(@Type int type, int frameRateHint) {
        switch (type) {
            case Type.NO_ADJUSTMENT:
                return Math.min(frameRateHint, 30); // 30 = MAXIMUM_INITIAL_FPS
            case Type.FRAMERATE_ADJUSTMENT:
                return FRAMERATE_ADJUSTMENT_BITRATE_ADJUSTMENT_FPS;
        }
        return 0;
    }
}
