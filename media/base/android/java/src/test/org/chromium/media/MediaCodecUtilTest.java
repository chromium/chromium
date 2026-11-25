// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.media.MediaFormat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowMediaCodec;
import org.robolectric.shadows.ShadowMediaCodecList;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.media.MediaCodecUtil.CodecCreationInfo;

/** Tests for MediaCodecUtil. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaCodecUtilTest {
    @Test
    public void testEmptyDecoderNameReturnsNullWithoutLoggingFailure() {
        MediaCodecUtilJni.setInstanceForTesting(
                new MediaCodecUtil.Natives() {
                    @Override
                    public boolean isDecoderSupportedForDevice(String mimeType) {
                        return true;
                    }

                    @Override
                    public int estimateVideoMaxInputSize(String mimeType, int width, int height) {
                        return 123456;
                    }
                });
        ShadowMediaCodec.clearCodecs();
        ShadowMediaCodecList.reset();
        ShadowLog.clear();

        CodecCreationInfo[] results = {
            MediaCodecUtil.createDecoder(MediaFormat.MIMETYPE_VIDEO_HEVC, CodecType.SOFTWARE, null),
            MediaCodecUtil.createDecoder(MediaFormat.MIMETYPE_VIDEO_HEVC, CodecType.SECURE, null),
            MediaCodecUtil.createDecoderByName(MediaFormat.MIMETYPE_VIDEO_HEVC, ""),
        };
        for (CodecCreationInfo info : results) {
            assertNull(info.mediaCodec);
            assertFalse(info.supportsAdaptivePlayback);
        }
        for (ShadowLog.LogItem item : ShadowLog.getLogs()) {
            assertFalse(
                    item.tag.equals("cr_MediaCodecUtil")
                            && item.msg.contains("Failed to create MediaCodec"));
        }
    }
}
