// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodec;
import android.util.SparseArray;

import org.jni_zero.JNINamespace;

import org.chromium.base.Log;

import java.nio.ByteBuffer;

/**
 * This class extends MediaCodecBridge for encoding processing.
 * As to H264, WebRTC requires that each IDR/keyframe should have SPS/PPS at the beginning.
 * Unlike other HW/SW H264 codec implementations, MediaCodec will generate a separate config
 * frame with SPS/PPS as the first frame and won't include them in the following keyframes.
 * So here we append the SPS/PPS NALs at the start of each keyframe.
 */
@JNINamespace("media")
class MediaCodecEncoder extends MediaCodecBridge {
    private static final String TAG = "MediaCodecEncoder";

    // Output buffers mapping with MediaCodec output buffers for the possible frame-merging.
    private SparseArray<ByteBuffer> mOutputBuffers = new SparseArray<>();
    // SPS and PPS NALs (Config frame).
    private ByteBuffer mConfigData;

    protected MediaCodecEncoder(MediaCodec mediaCodec, @BitrateAdjuster.Type int bitrateAdjuster) {
        super(mediaCodec, bitrateAdjuster, false);
    }

    @Override
    protected ByteBuffer getOutputBuffer(int index) {
        return mOutputBuffers.get(index);
    }

    @Override
    protected void releaseOutputBuffer(int index, boolean render) {
        try {
            mMediaCodec.releaseOutputBuffer(index, render);
            mOutputBuffers.remove(index);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to release output buffer", e);
        }
    }

    // Save the config frame(SPS/PPS NALs) and append it to each keyframe.
    // WrongConstant: MediaCodec#dequeueOutputBuffer returns the index when successful.
    @SuppressWarnings({"deprecation", "WrongConstant"})
    @Override
    protected int dequeueOutputBufferInternal(MediaCodec.BufferInfo info, long timeoutUs) {
        int indexOrStatus = -1;

        try {
            indexOrStatus = mMediaCodec.dequeueOutputBuffer(info, timeoutUs);

            ByteBuffer codecOutputBuffer = null;
            if (indexOrStatus >= 0) {
                boolean isConfigFrame = (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                if (isConfigFrame) {
                    Log.d(
                            TAG,
                            "Config frame generated. Offset: %d, size: %d",
                            info.offset,
                            info.size);
                    codecOutputBuffer = getMediaCodecOutputBuffer(indexOrStatus);
                    codecOutputBuffer.position(info.offset);
                    codecOutputBuffer.limit(info.offset + info.size);

                    mConfigData = ByteBuffer.allocateDirect(info.size);
                    mConfigData.put(codecOutputBuffer);
                    // Log few SPS header bytes to check profile and level.
                    StringBuilder spsData = new StringBuilder();
                    for (int i = 0; i < (info.size < 8 ? info.size : 8); i++) {
                        spsData.append(Integer.toHexString(mConfigData.get(i) & 0xff)).append(" ");
                    }
                    Log.i(TAG, "spsData: %s", spsData.toString());

                    // Release buffer back.
                    mMediaCodec.releaseOutputBuffer(indexOrStatus, false);
                    // Query next output.
                    indexOrStatus = mMediaCodec.dequeueOutputBuffer(info, timeoutUs);
                }
            }

            if (indexOrStatus >= 0) {
                codecOutputBuffer = getMediaCodecOutputBuffer(indexOrStatus);
                codecOutputBuffer.position(info.offset);
                codecOutputBuffer.limit(info.offset + info.size);

                // Check key frame flag.
                boolean isKeyFrame = (info.flags & MediaCodec.BUFFER_FLAG_SYNC_FRAME) != 0;
                if (isKeyFrame) {
                    Log.d(TAG, "Key frame generated");
                }
                final ByteBuffer frameBuffer;
                if (isKeyFrame && mConfigData != null) {
                    Log.d(
                            TAG,
                            "Appending config frame of size %d to output buffer with size %d",
                            mConfigData.capacity(),
                            info.size);
                    // For encoded key frame append SPS and PPS NALs at the start.
                    frameBuffer = ByteBuffer.allocateDirect(mConfigData.capacity() + info.size);
                    mConfigData.rewind();
                    frameBuffer.put(mConfigData);
                    frameBuffer.put(codecOutputBuffer);
                    frameBuffer.rewind();
                    info.offset = 0;
                    info.size += mConfigData.capacity();
                    mOutputBuffers.put(indexOrStatus, frameBuffer);
                } else {
                    mOutputBuffers.put(indexOrStatus, codecOutputBuffer);
                }
            }
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to dequeue output buffer", e);
        }

        return indexOrStatus;
    }

    // Call this function with catching IllegalStateException.
    @SuppressWarnings("deprecation")
    private ByteBuffer getMediaCodecOutputBuffer(int index) {
        ByteBuffer outputBuffer = super.getOutputBuffer(index);
        if (outputBuffer == null) {
            throw new IllegalStateException("Got null output buffer");
        }
        return outputBuffer;
    }
}
