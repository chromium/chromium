// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.media.MediaCodecInfo;
import android.media.MediaFormat;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.media.MediaCodecUtil.MimeTypes;

import java.nio.ByteBuffer;

@NullMarked
class MediaFormatBuilder {
    public static @Nullable MediaFormat createVideoDecoderFormat(
            String mime,
            int width,
            int height,
            byte[][] csds,
            @Nullable HdrMetadata hdrMetadata,
            boolean allowAdaptivePlayback,
            int profile) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        setCodecSpecificData(format, csds);
        if (hdrMetadata != null) {
            hdrMetadata.addMetadataToFormat(format);
        }
        addInputSizeInfoToFormat(format, allowAdaptivePlayback);
        addProfileInfoToFormat(format, profile);
        return format;
    }

    public static MediaFormat createAudioFormat(
            String mime,
            int sampleRate,
            int channelCount,
            byte[][] csds,
            boolean frameHasAdtsHeader) {
        MediaFormat format = MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
        setCodecSpecificData(format, csds);
        if (frameHasAdtsHeader) {
            format.setInteger(MediaFormat.KEY_IS_ADTS, 1);
        }
        return format;
    }

    private static void setCodecSpecificData(MediaFormat format, byte[][] csds) {
        // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
        // csd-1, and so on. See:
        // http://developer.android.com/reference/android/media/MediaCodec.html for details.
        for (int i = 0; i < csds.length; ++i) {
            if (csds[i].length == 0) continue;
            String name = "csd-" + i;
            format.setByteBuffer(name, ByteBuffer.wrap(csds[i]));
        }
    }

    // Use some heuristics to set KEY_MAX_INPUT_SIZE (the size of the input buffers).
    // Taken from exoplayer:
    // https://github.com/google/ExoPlayer/blob/8595c65678a181296cdf673eacb93d8135479340/library/src/main/java/com/google/android/exoplayer/MediaCodecVideoTrackRenderer.java
    private static void addInputSizeInfoToFormat(
            MediaFormat format, boolean allowAdaptivePlayback) {
        if (allowAdaptivePlayback) {
            if (DisplayCompat.isTv(ContextUtils.getApplicationContext())) {
                // For now, only set max width and height to native resolution on TVs.
                // Some decoders on TVs interpret max width / height quite literally,
                // and a crash can occur if these are exceeded.
                MaxAnticipatedResolutionEstimator.Resolution resolution =
                        MaxAnticipatedResolutionEstimator.getScreenResolution(format);

                format.setInteger(MediaFormat.KEY_MAX_WIDTH, resolution.getWidth());
                format.setInteger(MediaFormat.KEY_MAX_HEIGHT, resolution.getHeight());
            } else {
                format.setInteger(
                        MediaFormat.KEY_MAX_WIDTH, format.getInteger(MediaFormat.KEY_WIDTH));
                format.setInteger(
                        MediaFormat.KEY_MAX_HEIGHT, format.getInteger(MediaFormat.KEY_HEIGHT));
            }
        }
        if (format.containsKey(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)) {
            // Already set. The source of the format may know better, so do nothing.
            return;
        }

        // The size calculations break down at small sizes, so use at least 128x128.
        int maxHeight = Math.max(128, format.getInteger(MediaFormat.KEY_HEIGHT));
        if (allowAdaptivePlayback && format.containsKey(MediaFormat.KEY_MAX_HEIGHT)) {
            maxHeight = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_HEIGHT));
        }
        int maxWidth = Math.max(128, format.getInteger(MediaFormat.KEY_WIDTH));
        if (allowAdaptivePlayback && format.containsKey(MediaFormat.KEY_MAX_WIDTH)) {
            maxWidth = Math.max(maxWidth, format.getInteger(MediaFormat.KEY_MAX_WIDTH));
        }
        int maxInputSize =
                MediaCodecUtilJni.get()
                        .estimateVideoMaxInputSize(
                                assumeNonNull(format.getString(MediaFormat.KEY_MIME)),
                                maxWidth,
                                maxHeight);
        if (maxInputSize > 0) {
            format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, maxInputSize);
        }
    }

    private static void addProfileInfoToFormat(MediaFormat format, int profile) {
        if (MimeTypes.VIDEO_DV.equals(format.getString(MediaFormat.KEY_MIME))) {
            if (profile == VideoCodecProfile.DOLBYVISION_PROFILE5) {
                format.setInteger(
                        MediaFormat.KEY_PROFILE,
                        MediaCodecInfo.CodecProfileLevel.DolbyVisionProfileDvheStn);
            } else if (profile == VideoCodecProfile.DOLBYVISION_PROFILE8) {
                format.setInteger(
                        MediaFormat.KEY_PROFILE,
                        MediaCodecInfo.CodecProfileLevel.DolbyVisionProfileDvheSt);
            }
        }
    }
}
