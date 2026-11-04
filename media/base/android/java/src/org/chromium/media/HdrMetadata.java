// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.media.MediaFormat;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@JNINamespace("media")
@NullMarked
class HdrMetadata {
    @JniType("::media::JniHdrMetadata")
    interface NativeJniHdrMetadata extends JniTypeToken {}

    private static final int MAX_CHROMATICITY = 50000; // Defined in CTA-861.3.

    private @Nullable JniRawPtr<NativeJniHdrMetadata> mNativeJniHdrMetadata;
    private final Object mLock = new Object();

    @CalledByNative
    static HdrMetadata create(JniRawPtr<NativeJniHdrMetadata> nativeRef) {
        return new HdrMetadata(nativeRef);
    }

    @VisibleForTesting
    HdrMetadata() {
        // Used for testing only.
        mNativeJniHdrMetadata = null;
    }

    private HdrMetadata(JniRawPtr<NativeJniHdrMetadata> nativeRef) {
        assert nativeRef != null;
        mNativeJniHdrMetadata = nativeRef;
    }

    @CalledByNative
    void close() {
        synchronized (mLock) {
            if (mNativeJniHdrMetadata != null) {
                mNativeJniHdrMetadata.release();
                mNativeJniHdrMetadata = null;
            }
        }
    }

    public void addMetadataToFormat(MediaFormat format) {
        synchronized (mLock) {
            assert mNativeJniHdrMetadata != null;
            JniPtr<NativeJniHdrMetadata> nativeMetadata = assumeNonNull(mNativeJniHdrMetadata);

            ByteBuffer hdrStaticInfo = ByteBuffer.wrap(new byte[25]);
            hdrStaticInfo.order(ByteOrder.LITTLE_ENDIAN);
            hdrStaticInfo.put((byte) 0); // Type.
            hdrStaticInfo.putShort(
                    (short) ((primaryRChromaticityX(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((primaryRChromaticityY(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((primaryGChromaticityX(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((primaryGChromaticityY(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((primaryBChromaticityX(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((primaryBChromaticityY(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((whitePointChromaticityX(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort(
                    (short) ((whitePointChromaticityY(nativeMetadata) * MAX_CHROMATICITY) + 0.5f));
            hdrStaticInfo.putShort((short) (maxColorVolumeLuminance(nativeMetadata) + 0.5f));
            hdrStaticInfo.putShort((short) (minColorVolumeLuminance(nativeMetadata) + 0.5f));
            hdrStaticInfo.putShort((short) maxContentLuminance(nativeMetadata));
            hdrStaticInfo.putShort((short) maxFrameAverageLuminance(nativeMetadata));

            hdrStaticInfo.rewind();
            format.setByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO, hdrStaticInfo);
        }
    }

    private float primaryRChromaticityX(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryRChromaticityX(nativeMetadata);
    }

    private float primaryRChromaticityY(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryRChromaticityY(nativeMetadata);
    }

    private float primaryGChromaticityX(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryGChromaticityX(nativeMetadata);
    }

    private float primaryGChromaticityY(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryGChromaticityY(nativeMetadata);
    }

    private float primaryBChromaticityX(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryBChromaticityX(nativeMetadata);
    }

    private float primaryBChromaticityY(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().primaryBChromaticityY(nativeMetadata);
    }

    private float whitePointChromaticityX(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().whitePointChromaticityX(nativeMetadata);
    }

    private float whitePointChromaticityY(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().whitePointChromaticityY(nativeMetadata);
    }

    private float maxColorVolumeLuminance(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().maxColorVolumeLuminance(nativeMetadata);
    }

    private float minColorVolumeLuminance(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().minColorVolumeLuminance(nativeMetadata);
    }

    private int maxContentLuminance(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().maxContentLuminance(nativeMetadata);
    }

    private int maxFrameAverageLuminance(JniPtr<NativeJniHdrMetadata> nativeMetadata) {
        return HdrMetadataJni.get().maxFrameAverageLuminance(nativeMetadata);
    }

    @NativeMethods
    interface Natives {
        float primaryRChromaticityX(JniPtr<NativeJniHdrMetadata> self);

        float primaryRChromaticityY(JniPtr<NativeJniHdrMetadata> self);

        float primaryGChromaticityX(JniPtr<NativeJniHdrMetadata> self);

        float primaryGChromaticityY(JniPtr<NativeJniHdrMetadata> self);

        float primaryBChromaticityX(JniPtr<NativeJniHdrMetadata> self);

        float primaryBChromaticityY(JniPtr<NativeJniHdrMetadata> self);

        float whitePointChromaticityX(JniPtr<NativeJniHdrMetadata> self);

        float whitePointChromaticityY(JniPtr<NativeJniHdrMetadata> self);

        float maxColorVolumeLuminance(JniPtr<NativeJniHdrMetadata> self);

        float minColorVolumeLuminance(JniPtr<NativeJniHdrMetadata> self);

        int maxContentLuminance(JniPtr<NativeJniHdrMetadata> self);

        int maxFrameAverageLuminance(JniPtr<NativeJniHdrMetadata> self);
    }
}
