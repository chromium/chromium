// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaFormat;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

@JNINamespace("media")
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

            // TODO(sandv): Use color space matrix when android has support for it.
            int colorStandard = getColorStandard();
            if (colorStandard != -1) {
                format.setInteger(MediaFormat.KEY_COLOR_STANDARD, colorStandard);
            }
            int colorTransfer = getColorTransfer();
            if (colorTransfer != -1) {
                format.setInteger(MediaFormat.KEY_COLOR_TRANSFER, colorTransfer);
            }
            int colorRange = getColorRange();
            if (colorRange != -1) format.setInteger(MediaFormat.KEY_COLOR_RANGE, colorRange);

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

    private int getColorStandard() {
        // media/base/video_color_space.h
        switch (HdrMetadataJni.get().primaries(mNativeJniHdrMetadata, HdrMetadata.this)) {
            case 1:
                return MediaFormat.COLOR_STANDARD_BT709;
            case 4: // BT.470M.
            case 5: // BT.470BG.
            case 6: // SMPTE 170M.
            case 7: // SMPTE 240M.
                return MediaFormat.COLOR_STANDARD_BT601_NTSC;
            case 9:
                return MediaFormat.COLOR_STANDARD_BT2020;
            default:
                return -1;
        }
    }

    private int getColorTransfer() {
        // media/base/video_color_space.h
        switch (HdrMetadataJni.get().colorTransfer(mNativeJniHdrMetadata, HdrMetadata.this)) {
            case 1: // BT.709.
            case 6: // SMPTE 170M.
            case 7: // SMPTE 240M.
                return MediaFormat.COLOR_TRANSFER_SDR_VIDEO;
            case 8:
                return MediaFormat.COLOR_TRANSFER_LINEAR;
            case 16:
                return MediaFormat.COLOR_TRANSFER_ST2084;
            case 18:
                return MediaFormat.COLOR_TRANSFER_HLG;
            default:
                return -1;
        }
    }

    private int getColorRange() {
        // media/base/video_color_space.h
        switch (HdrMetadataJni.get().range(mNativeJniHdrMetadata, HdrMetadata.this)) {
            case 1:
                return MediaFormat.COLOR_RANGE_LIMITED;
            case 2:
                return MediaFormat.COLOR_RANGE_FULL;
            default:
                return -1;
        }
    }

    private float primaryRChromaticityX() {
        return HdrMetadataJni.get().primaryRChromaticityX(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float primaryRChromaticityY() {
        return HdrMetadataJni.get().primaryRChromaticityY(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float primaryGChromaticityX() {
        return HdrMetadataJni.get().primaryGChromaticityX(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float primaryGChromaticityY() {
        return HdrMetadataJni.get().primaryGChromaticityY(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float primaryBChromaticityX() {
        return HdrMetadataJni.get().primaryBChromaticityX(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float primaryBChromaticityY() {
        return HdrMetadataJni.get().primaryBChromaticityY(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float whitePointChromaticityX() {
        return HdrMetadataJni.get()
                .whitePointChromaticityX(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float whitePointChromaticityY() {
        return HdrMetadataJni.get()
                .whitePointChromaticityY(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float maxColorVolumeLuminance() {
        return HdrMetadataJni.get()
                .maxColorVolumeLuminance(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private float minColorVolumeLuminance() {
        return HdrMetadataJni.get()
                .minColorVolumeLuminance(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private int maxContentLuminance() {
        return HdrMetadataJni.get().maxContentLuminance(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    private int maxFrameAverageLuminance() {
        return HdrMetadataJni.get()
                .maxFrameAverageLuminance(mNativeJniHdrMetadata, HdrMetadata.this);
    }

    @NativeMethods
    interface Natives {
        int primaries(long nativeJniHdrMetadata, HdrMetadata caller);

        int colorTransfer(long nativeJniHdrMetadata, HdrMetadata caller);

        int range(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryRChromaticityX(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryRChromaticityY(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryGChromaticityX(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryGChromaticityY(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryBChromaticityX(long nativeJniHdrMetadata, HdrMetadata caller);

        float primaryBChromaticityY(long nativeJniHdrMetadata, HdrMetadata caller);

        float whitePointChromaticityX(long nativeJniHdrMetadata, HdrMetadata caller);

        float whitePointChromaticityY(long nativeJniHdrMetadata, HdrMetadata caller);

        float maxColorVolumeLuminance(long nativeJniHdrMetadata, HdrMetadata caller);

        float minColorVolumeLuminance(long nativeJniHdrMetadata, HdrMetadata caller);

        int maxContentLuminance(long nativeJniHdrMetadata, HdrMetadata caller);

        int maxFrameAverageLuminance(long nativeJniHdrMetadata, HdrMetadata caller);
    }
}
