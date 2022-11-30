// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecList;
import android.os.Build;
import android.util.Range;

import androidx.annotation.RequiresApi;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.HashSet;

/**
 * A collection of SDK based helper functions for the NDK VideoEncoder that
 * unfortunately don't have NDK equivalents. Only called from the GPU process,
 * so doesn't need to be tagged with MainDex.
 */
@JNINamespace("media")
class VideoEncodeAcceleratorUtil {
    private static final String TAG = "VideoEncodeAcceleratorUtil";

    private static final String[] SUPPORTED_TYPES = {
            MediaCodecUtil.MimeTypes.VIDEO_VP8,
            MediaCodecUtil.MimeTypes.VIDEO_VP9,
            MediaCodecUtil.MimeTypes.VIDEO_AV1,
            MediaCodecUtil.MimeTypes.VIDEO_H264,
    };

    private static class SupportedProfileAdapter {
        public int profile;
        public int maxWidth;
        public int maxHeight;
        public int minWidth;
        public int minHeight;
        public int maxFramerateNumerator;
        public int maxFramerateDenominator;
        public boolean supportsCbr;
        public boolean supportsVbr;

        @CalledByNative("SupportedProfileAdapter")
        public int getProfile() {
            return this.profile;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMaxWidth() {
            return this.maxWidth;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMaxHeight() {
            return this.maxHeight;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMinWidth() {
            return this.minWidth;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMinHeight() {
            return this.minHeight;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMaxFramerateNumerator() {
            return this.maxFramerateNumerator;
        }

        @CalledByNative("SupportedProfileAdapter")
        public int getMaxFramerateDenominator() {
            return this.maxFramerateDenominator;
        }

        @CalledByNative("SupportedProfileAdapter")
        public boolean supportsCbr() {
            return this.supportsCbr;
        }

        @CalledByNative("SupportedProfileAdapter")
        public boolean supportsVbr() {
            return this.supportsVbr;
        }
    }

    // Currently our encoder only supports NV12.
    private static boolean hasSupportedColorFormat(int[] colorFormats) {
        for (int format : colorFormats) {
            if (format == MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar) {
                return true;
            }
        }
        return false;
    }

    // Chromium doesn't bundle a software encoder for H.264 so allow it.
    private static boolean requiresHardware(String type) {
        return !type.equalsIgnoreCase(MediaCodecUtil.MimeTypes.VIDEO_H264);
    }

    /**
     * Returns an array of SupportedProfileAdapter entries since the NDK
     * doesn't provide this functionality :/
     */
    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.Q)
    private static SupportedProfileAdapter[] getSupportedProfiles() {
        MediaCodecInfo[] codecList;
        try {
            codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos();
        } catch (Throwable e) {
            // Swallow the exception due to bad Android implementation and pretend
            // MediaCodecList is not supported.
            return null;
        }

        ArrayList<SupportedProfileAdapter> hardwareProfiles =
                new ArrayList<SupportedProfileAdapter>();
        ArrayList<SupportedProfileAdapter> softwareProfiles =
                new ArrayList<SupportedProfileAdapter>();
        HashSet<Integer> hardwareProfileSet = new HashSet<Integer>();

        for (String type : SUPPORTED_TYPES) {
            for (MediaCodecInfo info : codecList) {
                if (!info.isEncoder()) continue;
                if (!info.isHardwareAccelerated() && requiresHardware(type)) continue;

                MediaCodecInfo.CodecCapabilities capabilities = null;
                try {
                    capabilities = info.getCapabilitiesForType(type);
                } catch (IllegalArgumentException e) {
                    // Type is not supported.
                    continue;
                }

                if (!hasSupportedColorFormat(capabilities.colorFormats)) continue;

                MediaCodecInfo.EncoderCapabilities encoderCapabilities =
                        capabilities.getEncoderCapabilities();
                boolean supportsCbr = encoderCapabilities.isBitrateModeSupported(
                        MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR);
                boolean supportsVbr = encoderCapabilities.isBitrateModeSupported(
                        MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR);

                MediaCodecInfo.VideoCapabilities videoCapabilities =
                        capabilities.getVideoCapabilities();

                // In landscape mode, width is always larger than height, so first get the
                // maximum width and then the height range supported for that width.
                Range<Integer> supportedWidths = videoCapabilities.getSupportedWidths();
                Range<Integer> supportedHeights =
                        videoCapabilities.getSupportedHeightsFor(supportedWidths.getUpper());
                boolean needsPortraitEntry =
                        !supportedHeights.getUpper().equals(supportedWidths.getUpper())
                        && videoCapabilities.isSizeSupported(
                                supportedHeights.getUpper(), supportedWidths.getUpper());

                // The frame rate entry in the supported profile is independent of the resolution
                // range, so we don't query based on the maximum resolution.
                Range<Integer> supportedFrameRates = videoCapabilities.getSupportedFrameRates();

                // Since the supported profiles interface doesn't support levels, we just attach
                // the same min/max to every profile.
                HashSet<Integer> supportedProfiles = new HashSet<Integer>();
                int codec = CodecProfileLevelList.getCodecFromMime(type);
                for (CodecProfileLevel cpl : capabilities.profileLevels) {
                    try {
                        supportedProfiles.add(
                                CodecProfileLevelList.mediaCodecProfileToChromiumMediaProfile(
                                        codec, cpl.profile));
                    } catch (RuntimeException e) {
                        continue;
                    }
                }

                if (info.isHardwareAccelerated()) hardwareProfileSet.addAll(supportedProfiles);
                ArrayList<SupportedProfileAdapter> profiles =
                        info.isHardwareAccelerated() ? hardwareProfiles : softwareProfiles;
                for (int mediaProfile : supportedProfiles) {
                    SupportedProfileAdapter profile = new SupportedProfileAdapter();

                    profile.profile = mediaProfile;
                    profile.minWidth = supportedWidths.getLower();
                    profile.minHeight = supportedHeights.getLower();
                    profile.maxWidth = supportedWidths.getUpper();
                    profile.maxHeight = supportedHeights.getUpper();
                    profile.maxFramerateNumerator = supportedFrameRates.getUpper();
                    profile.maxFramerateDenominator = 1;
                    profile.supportsCbr = supportsCbr;
                    profile.supportsVbr = supportsVbr;
                    profiles.add(profile);

                    // Invert min/max height/width for a portrait mode entry if needed.
                    if (needsPortraitEntry) {
                        profile = new SupportedProfileAdapter();

                        profile.profile = mediaProfile;
                        profile.minWidth = supportedHeights.getLower();
                        profile.minHeight = supportedWidths.getLower();
                        profile.maxWidth = supportedHeights.getUpper();
                        profile.maxHeight = supportedWidths.getUpper();
                        profile.maxFramerateNumerator = supportedFrameRates.getUpper();
                        profile.maxFramerateDenominator = 1;
                        profile.supportsCbr = supportsCbr;
                        profile.supportsVbr = supportsVbr;
                        profiles.add(profile);
                    }
                }
            }
        }

        // For allowed software codecs, if we don't also have hardware support, add the software
        // capabilities.
        ArrayList<SupportedProfileAdapter> profiles = hardwareProfiles;
        for (SupportedProfileAdapter profile : softwareProfiles) {
            if (!hardwareProfileSet.contains(profile.profile)) profiles.add(profile);
        }

        SupportedProfileAdapter[] profileArray = new SupportedProfileAdapter[profiles.size()];
        profiles.toArray(profileArray);
        return profileArray;
    }
}
