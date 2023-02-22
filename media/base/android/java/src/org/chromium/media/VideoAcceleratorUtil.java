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

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Locale;

/**
 * A collection of SDK based helper functions for retrieving supported profiles
 * for accelerated encoders and decoders from MediaCodecInfo. Only called from
 * the GPU process, so doesn't need to be tagged with MainDex.
 */
@JNINamespace("media")
class VideoAcceleratorUtil {
    private static final String TAG = "VAUtil";

    private static final String[] SUPPORTED_ENCODER_TYPES = {
            MediaCodecUtil.MimeTypes.VIDEO_VP8,
            MediaCodecUtil.MimeTypes.VIDEO_VP9,
            MediaCodecUtil.MimeTypes.VIDEO_AV1,
            MediaCodecUtil.MimeTypes.VIDEO_H264,
    };

    private static final String[] SUPPORTED_DECODER_TYPES = {
            MediaCodecUtil.MimeTypes.VIDEO_VP8,
            MediaCodecUtil.MimeTypes.VIDEO_VP9,
            MediaCodecUtil.MimeTypes.VIDEO_AV1,
            MediaCodecUtil.MimeTypes.VIDEO_H264,
            MediaCodecUtil.MimeTypes.VIDEO_HEVC,
            MediaCodecUtil.MimeTypes.VIDEO_DV,
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
        public String name;
        public boolean isSoftwareCodec;

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

        @CalledByNative("SupportedProfileAdapter")
        public String getName() {
            return this.name;
        }

        @CalledByNative("SupportedProfileAdapter")
        public boolean isSoftwareCodec() {
            return this.isSoftwareCodec;
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

    // Chromium doesn't bundle a software encoder or decoder for H.264 or H.265 so allow
    // usage of software codecs through MediaCodec for those codecs.
    private static boolean requiresHardware(String type) {
        return !type.equalsIgnoreCase(MediaCodecUtil.MimeTypes.VIDEO_H264)
                && !type.equalsIgnoreCase(MediaCodecUtil.MimeTypes.VIDEO_HEVC);
    }

    // H.264 high profile isn't required by Android platform, so we can only add support if
    // we know its supported by the underlying codec.
    private static boolean hasHighProfileSupport(String name) {
        var lowerName = name.toLowerCase(Locale.ROOT);

        // Some platforms seem to have a trailing `.` in the name...
        return lowerName.startsWith("omx.google.h264.decoder")
                || lowerName.startsWith("c2.android.avc.decoder");
    }

    // Return true if and only if this is a low latency decoder.
    private static boolean isLowLatency(String name) {
        var lowerName = name.toLowerCase(Locale.ROOT);
        // This is usually a hw decoder provided by the OEM vendors.
        return lowerName.endsWith(".low_latency");
    }

    /**
     * Returns an array of SupportedProfileAdapter entries since the NDK
     * doesn't provide this functionality :/
     */
    @CalledByNative
    @RequiresApi(Build.VERSION_CODES.Q)
    private static SupportedProfileAdapter[] getSupportedEncoderProfiles() {
        MediaCodecInfo[] codecList;
        try {
            codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos();
        } catch (Throwable e) {
            // Swallow the exception due to bad Android implementation and pretend
            // MediaCodecList is not supported.
            Log.e(TAG, "Unable to retrieve MediaCodecInfo: ", e);
            return null;
        }

        ArrayList<SupportedProfileAdapter> hardwareProfiles =
                new ArrayList<SupportedProfileAdapter>();
        ArrayList<SupportedProfileAdapter> softwareProfiles =
                new ArrayList<SupportedProfileAdapter>();

        for (String type : SUPPORTED_ENCODER_TYPES) {
            for (MediaCodecInfo info : codecList) {
                if (info.isAlias()) continue; // Skip duplicates.
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
                        // This means mediaCodecProfileToChromiumMediaProfile() needs updating.
                        Log.w(TAG, "Unknown profile: " + cpl.profile + " for codec " + type);
                        continue;
                    }
                }

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
                    profile.name = info.getName();
                    profile.isSoftwareCodec = info.isSoftwareOnly();
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
                        profile.name = info.getName();
                        profile.isSoftwareCodec = info.isSoftwareOnly();
                        profiles.add(profile);
                    }
                }
            }
        }

        // Insert all software codecs after the hardware support.
        ArrayList<SupportedProfileAdapter> profiles = hardwareProfiles;
        profiles.addAll(softwareProfiles);

        SupportedProfileAdapter[] profileArray = new SupportedProfileAdapter[profiles.size()];
        profiles.toArray(profileArray);
        return profileArray;
    }

    /**
     * Returns an array of SupportedProfileAdapter entries since the NDK
     * doesn't provide this functionality :/
     */
    @CalledByNative
    private static SupportedProfileAdapter[] getSupportedDecoderProfiles() {
        MediaCodecInfo[] codecList;
        try {
            codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos();
        } catch (Throwable e) {
            // Swallow the exception due to bad Android implementation and pretend
            // MediaCodecList is not supported.
            Log.e(TAG, "Unable to retrieve MediaCodecInfo: ", e);
            return null;
        }

        ArrayList<SupportedProfileAdapter> profiles = new ArrayList<SupportedProfileAdapter>();

        boolean isAtLeastQ = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
        for (String type : SUPPORTED_DECODER_TYPES) {
            for (MediaCodecInfo info : codecList) {
                // Skip duplicates. Harmless, but pollutes chrome://gpu
                if (isAtLeastQ && info.isAlias()) continue;
                if (info.isEncoder()) continue;
                // Skip low latency codec in case duplication.
                if (isLowLatency(info.getName())) continue;

                MediaCodecInfo.CodecCapabilities capabilities = null;
                try {
                    capabilities = info.getCapabilitiesForType(type);
                } catch (IllegalArgumentException e) {
                    // Type is not supported.
                    continue;
                }

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
                        // This means mediaCodecProfileToChromiumMediaProfile() needs updating.
                        Log.w(TAG, "Unknown profile: " + cpl.profile + " for codec " + type);
                        continue;
                    }
                }

                // Not all platforms seem to have a populated `profileLevels`, e.g., the
                // x86 emulator. In these cases, populate whats required by Android:
                // https://developer.android.com/guide/topics/media/media-formats
                if (supportedProfiles.isEmpty()) {
                    Log.d(TAG,
                            "CodecCapabilities.profileLevels is missing for codec " + type
                                    + ". Assuming default support.");
                    switch (codec) {
                        case VideoCodec.VP8:
                            supportedProfiles.add(VideoCodecProfile.VP8PROFILE_ANY);
                            break;
                        case VideoCodec.VP9:
                            supportedProfiles.add(VideoCodecProfile.VP9PROFILE_PROFILE0);
                            break;
                        case VideoCodec.HEVC:
                            supportedProfiles.add(VideoCodecProfile.HEVCPROFILE_MAIN);
                            break;
                        case VideoCodec.H264:
                            supportedProfiles.add(VideoCodecProfile.H264PROFILE_BASELINE);
                            supportedProfiles.add(VideoCodecProfile.H264PROFILE_MAIN);
                            break;
                        case VideoCodec.AV1:
                            supportedProfiles.add(VideoCodecProfile.AV1PROFILE_PROFILE_MAIN);
                            break;
                    }
                }

                // Prior to Oreo, high profile support wasn't advertised properly.
                if (codec == VideoCodec.H264 && Build.VERSION.SDK_INT < Build.VERSION_CODES.O
                        && hasHighProfileSupport(info.getName())) {
                    supportedProfiles.add(VideoCodecProfile.H264PROFILE_HIGH);
                }

                boolean isSoftwareCodec = MediaCodecUtil.isSoftwareCodec(info);
                for (int mediaProfile : supportedProfiles) {
                    SupportedProfileAdapter profile = new SupportedProfileAdapter();
                    profile.profile = mediaProfile;
                    profile.minWidth = supportedWidths.getLower();
                    profile.minHeight = supportedHeights.getLower();
                    profile.maxWidth = supportedWidths.getUpper();
                    profile.maxHeight = supportedHeights.getUpper();
                    profile.isSoftwareCodec = isSoftwareCodec;
                    profiles.add(profile);

                    Log.d(TAG,
                            "Support: name=" + info.getName() + ", profile=" + mediaProfile
                                    + ", min=" + profile.minWidth + "x" + profile.minHeight
                                    + ", max=" + profile.maxWidth + "x" + profile.maxHeight
                                    + ", is_sw=" + profile.isSoftwareCodec);

                    // Invert min/max height/width for a portrait mode entry if needed.
                    if (needsPortraitEntry) {
                        profile = new SupportedProfileAdapter();

                        profile.profile = mediaProfile;
                        profile.minWidth = supportedHeights.getLower();
                        profile.minHeight = supportedWidths.getLower();
                        profile.maxWidth = supportedHeights.getUpper();
                        profile.maxHeight = supportedWidths.getUpper();
                        profile.isSoftwareCodec = isSoftwareCodec;
                        profiles.add(profile);
                    }
                }
            }
        }

        SupportedProfileAdapter[] profileArray = new SupportedProfileAdapter[profiles.size()];
        profiles.toArray(profileArray);
        return profileArray;
    }
}
