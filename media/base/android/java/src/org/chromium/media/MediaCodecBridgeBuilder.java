// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodec;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.view.Surface;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.media.MediaCodecUtil.CodecCreationInfo;
import org.chromium.media.MediaCodecUtil.MimeTypes;

@JNINamespace("media")
@MainDex
class MediaCodecBridgeBuilder {
    private static final String TAG = "MediaCodecBridge";

    @CalledByNative
    static MediaCodecBridge createVideoDecoder(String mime, @CodecType int codecType,
            MediaCrypto mediaCrypto, int width, int height, Surface surface, byte[] csd0,
            byte[] csd1, HdrMetadata hdrMetadata, boolean allowAdaptivePlayback,
            boolean useAsyncApi) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec video decoder, mime %s", mime);
            info = MediaCodecUtil.createDecoder(mime, codecType, mediaCrypto);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec video decoder: %s, codecType: %d", mime,
                    codecType, e);
        }

        if (info.mediaCodec == null) return null;

        MediaCodecBridge bridge =
                new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster, useAsyncApi);
        byte[][] csds = {csd0, csd1};
        MediaFormat format = MediaFormatBuilder.createVideoDecoderFormat(mime, width, height, csds,
                hdrMetadata, info.supportsAdaptivePlayback && allowAdaptivePlayback);

        if (!bridge.configureVideo(format, surface, mediaCrypto, 0)) return null;

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }

    @CalledByNative
    static MediaCodecBridge createVideoEncoder(String mime, int width, int height, int bitRate,
            int frameRate, int iFrameInterval, int colorFormat) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec video encoder, mime %s", mime);
            info = MediaCodecUtil.createEncoder(mime);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec video encoder: %s", mime, e);
        }

        if (info.mediaCodec == null) return null;

        // Create MediaCodecEncoder for H264 to meet WebRTC requirements to IDR/keyframes.
        // See https://crbug.com/761336 for more details.
        MediaCodecBridge bridge = mime.equals(MimeTypes.VIDEO_H264)
                ? new MediaCodecEncoder(info.mediaCodec, info.bitrateAdjuster)
                : new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster, false);
        MediaFormat format = MediaFormatBuilder.createVideoEncoderFormat(mime, width, height,
                bitRate, BitrateAdjuster.getInitialFrameRate(info.bitrateAdjuster, frameRate),
                iFrameInterval, colorFormat, info.supportsAdaptivePlayback);

        if (!bridge.configureVideo(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)) {
            return null;
        }

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }

    @CalledByNative
    static MediaCodecBridge createAudioDecoder(String mime, MediaCrypto mediaCrypto, int sampleRate,
            int channelCount, byte[] csd0, byte[] csd1, byte[] csd2, boolean frameHasAdtsHeader,
            boolean useAsyncApi) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec audio decoder, mime %s", mime);
            info = MediaCodecUtil.createDecoder(mime, CodecType.ANY, mediaCrypto);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec audio decoder: %s", mime, e);
        }

        if (info.mediaCodec == null) return null;

        MediaCodecBridge bridge =
                new MediaCodecBridge(info.mediaCodec, info.bitrateAdjuster, useAsyncApi);
        byte[][] csds = {csd0, csd1, csd2};
        MediaFormat format = MediaFormatBuilder.createAudioFormat(
                mime, sampleRate, channelCount, csds, frameHasAdtsHeader);

        if (!bridge.configureAudio(format, mediaCrypto, 0)) return null;

        if (!bridge.start()) {
            bridge.release();
            return null;
        }
        return bridge;
    }
}
