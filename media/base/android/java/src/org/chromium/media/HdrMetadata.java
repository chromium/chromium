// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaFormat;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@JNINamespace("media")
@NullMarked
class HdrMetadata {
    private static final int MAX_CHROMATICITY = 50000; // Defined in CTA-861.3.

    private long mNativeJniHdrMetadata;
    private final Object mLock = new Object();

    @CalledByNative
    private static HdrMetadata create(long nativeRef) {
        return new HdrMetadata(nativeRef);
    }

    @VisibleForTesting
    HdrMetadata() {
        // Used for testing only.
        mNativeJniHdrMetadata = 0;
    }

    private HdrMetadata(long nativeRef) {
        assert nativeRef != 0;
        mNativeJniHdrMetadata = nativeRef;
    }

    @CalledByNative
    private void close() {
        synchronized (mLock) {
            mNativeJniHdrMetadata = 0;
        }
    }

    public void addMetadataToFormat(MediaFormat format) {
        synchronized (mLock) {
            assert mNativeJniHdrMetadata != 0;

            ByteBuffer hdrStaticInfo = ByteBuffer.wrap(new byte[25]);
            hdrStaticInfo.order(ByteOrder.LITTLE_ENDIAN);
            hdrStaticInfo.put((byte) 0); // Type.
            hdrStaticInfo.putShort((short) ((primaryRChromaticityX() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((primaryRChromaticityY() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((primaryGChromaticityX() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((primaryGChromaticityY() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((primaryBChromaticityX() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((primaryBChromaticityY() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((whitePointChromaticityX() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) ((whitePointChromaticityY() * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) (maxColorVolumeLuminance() + 0.5f));
            hdrStaticInfo.putShort((short) (minColorVolumeLuminance() + 0.5f));
            hdrStaticInfo.putShort((short) maxContentLuminance());
            hdrStaticInfo.putShort((short) maxFrameAverageLuminance());

            hdrStaticInfo.rewind();
            format.setByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO, hdrStaticInfo);
        }
    }

    private float primaryRChromaticityX() {
        return HdrMetadataJni.get().primaryRChromaticityX(mNativeJniHdrMetadata);
    }

    private float primaryRChromaticityY() {
        return HdrMetadataJni.get().primaryRChromaticityY(mNativeJniHdrMetadata);
    }

    private float primaryGChromaticityX() {
        return HdrMetadataJni.get().primaryGChromaticityX(mNativeJniHdrMetadata);
    }

    private float primaryGChromaticityY() {
        return HdrMetadataJni.get().primaryGChromaticityY(mNativeJniHdrMetadata);
    }

    private float primaryBChromaticityX() {
        return HdrMetadataJni.get().primaryBChromaticityX(mNativeJniHdrMetadata);
    }

    private float primaryBChromaticityY() {
        return HdrMetadataJni.get().primaryBChromaticityY(mNativeJniHdrMetadata);
    }

    private float whitePointChromaticityX() {
        return HdrMetadataJni.get().whitePointChromaticityX(mNativeJniHdrMetadata);
    }

    private float whitePointChromaticityY() {
        return HdrMetadataJni.get().whitePointChromaticityY(mNativeJniHdrMetadata);
    }

    private float maxColorVolumeLuminance() {
        return HdrMetadataJni.get().maxColorVolumeLuminance(mNativeJniHdrMetadata);
    }

    private float minColorVolumeLuminance() {
        return HdrMetadataJni.get().minColorVolumeLuminance(mNativeJniHdrMetadata);
    }

    private int maxContentLuminance() {
        return HdrMetadataJni.get().maxContentLuminance(mNativeJniHdrMetadata);
    }

    private int maxFrameAverageLuminance() {
        return HdrMetadataJni.get().maxFrameAverageLuminance(mNativeJniHdrMetadata);
    }

    @NativeMethods
    interface Natives {
        float primaryRChromaticityX(long nativeJniHdrMetadata);

        float primaryRChromaticityY(long nativeJniHdrMetadata);

        float primaryGChromaticityX(long nativeJniHdrMetadata);

        float primaryGChromaticityY(long nativeJniHdrMetadata);

        float primaryBChromaticityX(long nativeJniHdrMetadata);

        float primaryBChromaticityY(long nativeJniHdrMetadata);

        float whitePointChromaticityX(long nativeJniHdrMetadata);

        float whitePointChromaticityY(long nativeJniHdrMetadata);

        float maxColorVolumeLuminance(long nativeJniHdrMetadata);

        float minColorVolumeLuminance(long nativeJniHdrMetadata);

        int maxContentLuminance(long nativeJniHdrMetadata);

        int maxFrameAverageLuminance(long nativeJniHdrMetadata);
    }
}
