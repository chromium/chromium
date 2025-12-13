// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodec;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.view.Surface;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.media.MediaCodecUtil.CodecCreationInfo;

@JNINamespace("media")
@NullMarked
class MediaCodecBridgeBuilder {
    private static final String TAG = "MediaCodecBridge";

    @CalledByNative
    static @Nullable MediaCodecBridge createVideoDecoder(
            String mime,
            @CodecType int codecType,
            MediaCrypto mediaCrypto,
            int width,
            int height,
            Surface surface,
            byte[] csd0,
            byte[] csd1,
            HdrMetadata hdrMetadata,
            boolean allowAdaptivePlayback,
            boolean useAsyncApi,
            boolean useBlockModel,
            boolean useLowLatencyMode,
            String decoderName,
            int profile) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(
                    TAG,
                    "create MediaCodec video decoder, mime %s, decoder name %s, block_model=%b",
                    mime,
                    decoderName,
                    useBlockModel);
            if (!decoderName.isEmpty()) {
                info = MediaCodecUtil.createDecoderByName(mime, decoderName);
            } else {
                info = MediaCodecUtil.createDecoder(mime, codecType, mediaCrypto);
            }

            if (info.mediaCodec == null) return null;

            MediaCodecBridge bridge = new MediaCodecBridge(info.mediaCodec, useAsyncApi);
            byte[][] csds = {csd0, csd1};
            MediaFormat format =
                    MediaFormatBuilder.createVideoDecoderFormat(
                            mime,
                            width,
                            height,
                            csds,
                            hdrMetadata,
                            info.supportsAdaptivePlayback && allowAdaptivePlayback,
                            profile);
            assert format != null;
            if (useLowLatencyMode) {
                // Note: We only set this key when `useLowLatencyMode` is true
                // since setting it even to disabled (the default) breaks on
                // some devices (e.g., Android X86 emulator).
                format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);

                // TODO(crbug.com/439294798): This should probably be limited to Dimensity chips.
                if (decoderName.contains("mtk")) {
                    format.setInteger("vendor.mtk.vdec.cpu.boost.mode.value", 1);
                }
            }

            if (!bridge.configureVideo(
                    format,
                    surface,
                    mediaCrypto,
                    useBlockModel ? MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL : 0)) {
                return null;
            }

            if (!bridge.start()) {
                bridge.release();
                return null;
            }

            return bridge;
        } catch (Exception e) {
            Log.e(
                    TAG,
                    "Failed to create MediaCodec video decoder: %s, codecType: %d",
                    mime,
                    codecType,
                    e);
        }

        return null;
    }

    @CalledByNative
    static @Nullable MediaCodecBridge createAudioDecoder(
            String mime,
            MediaCrypto mediaCrypto,
            int sampleRate,
            int channelCount,
            byte[] csd0,
            byte[] csd1,
            byte[] csd2,
            boolean frameHasAdtsHeader,
            boolean useAsyncApi) {
        CodecCreationInfo info = new CodecCreationInfo();
        try {
            Log.i(TAG, "create MediaCodec audio decoder, mime %s", mime);
            info = MediaCodecUtil.createDecoder(mime, CodecType.ANY, mediaCrypto);

            if (info.mediaCodec == null) return null;

            MediaCodecBridge bridge = new MediaCodecBridge(info.mediaCodec, useAsyncApi);
            byte[][] csds = {csd0, csd1, csd2};
            MediaFormat format =
                    MediaFormatBuilder.createAudioFormat(
                            mime, sampleRate, channelCount, csds, frameHasAdtsHeader);

            if (!bridge.configureAudio(format, mediaCrypto, 0)) return null;

            if (!bridge.start()) {
                bridge.release();
                return null;
            }
            return bridge;

        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec audio decoder: %s", mime, e);
        }
        return null;
    }
}
