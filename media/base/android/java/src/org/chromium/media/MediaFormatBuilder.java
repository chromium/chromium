// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaFormat;
import android.os.Build;

import org.chromium.media.MediaCodecUtil.MimeTypes;

import java.nio.ByteBuffer;

class MediaFormatBuilder {
    public static MediaFormat createVideoDecoderFormat(String mime, int width, int height,
            byte[][] csds, HdrMetadata hdrMetadata, boolean allowAdaptivePlayback) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        if (format == null) return null;
        setCodecSpecificData(format, csds);
        if (hdrMetadata != null) {
            hdrMetadata.addMetadataToFormat(format);
        }
        addInputSizeInfoToFormat(format, allowAdaptivePlayback);
        return format;
    }

    public static MediaFormat createVideoEncoderFormat(String mime, int width, int height,
            int bitRate, int frameRate, int iFrameInterval, int colorFormat,
            boolean allowAdaptivePlayback) {
        MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
        addInputSizeInfoToFormat(format, allowAdaptivePlayback);
        return format;
    }

    public static MediaFormat createAudioFormat(String mime, int sampleRate, int channelCount,
            byte[][] csds, boolean frameHasAdtsHeader) {
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
            // The max size is a hint to the codec, and causes it to allocate more memory up front.
            // It still supports higher resolutions if they arrive. So, we try to ask only for the
            // initial size.
            // TODO(sanfin): The above statement that it supports higher resolutions if they arrive
            // is false on some platforms. Provide a better hint.
            format.setInteger(MediaFormat.KEY_MAX_WIDTH, format.getInteger(MediaFormat.KEY_WIDTH));
            format.setInteger(
                    MediaFormat.KEY_MAX_HEIGHT, format.getInteger(MediaFormat.KEY_HEIGHT));
        }
        if (format.containsKey(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)) {
            // Already set. The source of the format may know better, so do nothing.
            return;
        }
        int maxHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
        if (allowAdaptivePlayback && format.containsKey(MediaFormat.KEY_MAX_HEIGHT)) {
            maxHeight = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_HEIGHT));
        }
        int maxWidth = format.getInteger(MediaFormat.KEY_WIDTH);
        if (allowAdaptivePlayback && format.containsKey(MediaFormat.KEY_MAX_WIDTH)) {
            maxWidth = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_WIDTH));
        }
        int maxPixels;
        int minCompressionRatio;
        switch (format.getString(MediaFormat.KEY_MIME)) {
            case MimeTypes.VIDEO_H264:
                if ("BRAVIA 4K 2015".equals(Build.MODEL)) {
                    // The Sony BRAVIA 4k TV has input buffers that are too small for the calculated
                    // 4k video maximum input size, so use the default value.
                    return;
                }
                // Round up width/height to an integer number of macroblocks.
                maxPixels = ((maxWidth + 15) / 16) * ((maxHeight + 15) / 16) * 16 * 16;
                minCompressionRatio = 2;
                break;
            case MimeTypes.VIDEO_VP8:
                // VPX does not specify a ratio so use the values from the platform's SoftVPX.cpp.
                maxPixels = maxWidth * maxHeight;
                minCompressionRatio = 2;
                break;
            case MimeTypes.VIDEO_HEVC:
            case MimeTypes.VIDEO_VP9:
                maxPixels = maxWidth * maxHeight;
                minCompressionRatio = 4;
                break;
            default:
                // Leave the default max input size.
                return;
        }
        // Estimate the maximum input size assuming three channel 4:2:0 subsampled input frames.
        int maxInputSize = (maxPixels * 3) / (2 * minCompressionRatio);
        format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, maxInputSize);
    }
}
