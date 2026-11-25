// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Handler;
import android.view.Surface;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.MediaCodecInfoBuilder;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MediaCodecBridgeBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = MediaCodecBridgeBuilderTest.ShadowTestMediaCodec.class)
public class MediaCodecBridgeBuilderTest {
    private static final String VIDEO_MIME = MediaFormat.MIMETYPE_VIDEO_AVC;
    private static final String AUDIO_MIME = MediaFormat.MIMETYPE_AUDIO_OPUS;
    private static final byte[] EMPTY_CSD = new byte[0];

    @Implements(MediaCodec.class)
    public static class ShadowTestMediaCodec {
        static int sCreated;
        static int sReleased;
        static String sThrowOn;
        static boolean sThrowOnRelease;

        @Resetter
        public static void reset() {
            sCreated = 0;
            sReleased = 0;
            sThrowOn = null;
            sThrowOnRelease = false;
        }

        private static void maybeThrow(String method) {
            if (method.equals(sThrowOn)) throw new IllegalStateException(method + " failed");
        }

        @Implementation
        protected static MediaCodec createDecoderByType(String type) {
            maybeThrow("createDecoderByType");
            sCreated++;
            return Shadow.newInstanceOf(MediaCodec.class);
        }

        @Implementation
        protected static MediaCodec createByCodecName(String name) {
            maybeThrow("createByCodecName");
            sCreated++;
            return Shadow.newInstanceOf(MediaCodec.class);
        }

        @Implementation
        protected String getName() {
            return "test.decoder";
        }

        @Implementation
        protected MediaCodecInfo getCodecInfo() {
            maybeThrow("getCodecInfo");
            return MediaCodecInfoBuilder.newBuilder()
                    .setName("test.decoder")
                    .setIsEncoder(true)
                    .build();
        }

        @Implementation
        protected void setCallback(MediaCodec.Callback cb, Handler handler) {
            maybeThrow("setCallback");
        }

        @Implementation
        protected void configure(
                MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
            maybeThrow("configure");
        }

        @Implementation
        protected MediaFormat getInputFormat() {
            return new MediaFormat();
        }

        @Implementation
        protected void start() {
            maybeThrow("start");
        }

        @Implementation
        protected void release() {
            sReleased++;
            if (sThrowOnRelease) throw new RuntimeException("release failed");
        }
    }

    @Before
    public void setUp() {
        ShadowTestMediaCodec.reset();
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
    }

    private static MediaCodecBridge createVideoDecoder(
            HdrMetadata hdrMetadata, boolean useAsyncApi) {
        return MediaCodecBridgeBuilder.createVideoDecoder(
                VIDEO_MIME,
                CodecType.ANY,
                null,
                640,
                480,
                null,
                EMPTY_CSD,
                EMPTY_CSD,
                -1,
                -1,
                -1,
                hdrMetadata,
                false,
                useAsyncApi,
                false,
                false,
                "",
                VideoCodecProfile.H264PROFILE_BASELINE);
    }

    private static MediaCodecBridge createAudioDecoder(boolean useAsyncApi) {
        return MediaCodecBridgeBuilder.createAudioDecoder(
                AUDIO_MIME, null, 48000, 2, EMPTY_CSD, EMPTY_CSD, EMPTY_CSD, false, useAsyncApi);
    }

    private static void assertReleasedOnce(String label, Object result) {
        assertNull(label, result);
        assertEquals(label, 1, ShadowTestMediaCodec.sCreated);
        assertEquals(label, 1, ShadowTestMediaCodec.sReleased);
    }

    @Test
    public void testCreateVideoDecoderSuccess() {
        assertNotNull(createVideoDecoder(null, false));
        assertEquals(1, ShadowTestMediaCodec.sCreated);
        assertEquals(0, ShadowTestMediaCodec.sReleased);
    }

    @Test
    public void testCreateVideoDecoderFailuresReleaseCodec() {
        for (String method : new String[] {"configure", "start", "setCallback"}) {
            ShadowTestMediaCodec.reset();
            ShadowTestMediaCodec.sThrowOn = method;
            boolean useAsync = method.equals("setCallback");
            assertReleasedOnce(method, createVideoDecoder(null, useAsync));
        }

        ShadowTestMediaCodec.reset();
        HdrMetadata throwingHdr =
                new HdrMetadata() {
                    @Override
                    public void addMetadataToFormat(MediaFormat format) {
                        throw new RuntimeException("hdr failed");
                    }
                };
        assertReleasedOnce("hdrMetadata", createVideoDecoder(throwingHdr, false));

        ShadowTestMediaCodec.reset();
        ShadowTestMediaCodec.sThrowOn = "configure";
        ShadowTestMediaCodec.sThrowOnRelease = true;
        assertReleasedOnce("releaseThrows", createVideoDecoder(null, false));
    }

    @Test
    public void testCreateAudioDecoderSuccess() {
        assertNotNull(createAudioDecoder(false));
        assertEquals(1, ShadowTestMediaCodec.sCreated);
        assertEquals(0, ShadowTestMediaCodec.sReleased);
    }

    @Test
    public void testCreateAudioDecoderFailuresReleaseCodec() {
        for (String method : new String[] {"configure", "start", "setCallback"}) {
            ShadowTestMediaCodec.reset();
            ShadowTestMediaCodec.sThrowOn = method;
            boolean useAsync = method.equals("setCallback");
            assertReleasedOnce(method, createAudioDecoder(useAsync));
        }
    }

    @Test
    public void testCreateDecoderCodecInfoThrowsReleasesCodec() {
        ShadowTestMediaCodec.sThrowOn = "getCodecInfo";
        assertReleasedOnce(
                "createDecoder",
                MediaCodecUtil.createDecoder(VIDEO_MIME, CodecType.ANY, null).mediaCodec);

        ShadowTestMediaCodec.reset();
        ShadowTestMediaCodec.sThrowOn = "getCodecInfo";
        ShadowTestMediaCodec.sThrowOnRelease = true;
        assertReleasedOnce(
                "createDecoderReleaseThrows",
                MediaCodecUtil.createDecoder(VIDEO_MIME, CodecType.ANY, null).mediaCodec);

        ShadowTestMediaCodec.reset();
        ShadowTestMediaCodec.sThrowOn = "getCodecInfo";
        assertReleasedOnce(
                "createDecoderByName",
                MediaCodecUtil.createDecoderByName(VIDEO_MIME, "test.decoder").mediaCodec);

        ShadowTestMediaCodec.reset();
        ShadowTestMediaCodec.sThrowOn = "createDecoderByType";
        assertNull(
                "createDecoderByTypeThrows",
                MediaCodecUtil.createDecoder(VIDEO_MIME, CodecType.ANY, null).mediaCodec);
        assertEquals(0, ShadowTestMediaCodec.sCreated);
        assertEquals(0, ShadowTestMediaCodec.sReleased);

        ShadowTestMediaCodec.reset();
        ShadowTestMediaCodec.sThrowOn = "createByCodecName";
        assertNull(
                "createByCodecNameThrows",
                MediaCodecUtil.createDecoderByName(VIDEO_MIME, "test.decoder").mediaCodec);
        assertEquals(0, ShadowTestMediaCodec.sCreated);
        assertEquals(0, ShadowTestMediaCodec.sReleased);
    }
}
