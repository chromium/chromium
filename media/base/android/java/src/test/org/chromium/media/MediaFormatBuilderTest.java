// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.nio.ByteBuffer;

/** Tests for MediaFormatBuilder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaFormatBuilderTest {
    private static final String VIDEO_DECODER_MIME = MediaFormat.MIMETYPE_VIDEO_AVC;
    private static final int VIDEO_WIDTH = 640;
    private static final int VIDEO_HEIGHT = 480;
    private static final int VIDEO_PROFILE = VideoCodecProfile.H264PROFILE_BASELINE;
    private static final byte[] AVC_SPS_EXAMPLE = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0a, (byte) 0xf8, 0x41, (byte) 0xa2
    };
    private static final byte[] AVC_PPS_EXAMPLE = {
        0x00, 0x00, 0x00, 0x01, 0x68, (byte) 0xce, 0x38, (byte) 0x80
    };

    private static final String VIDEO_ENCODER_MIME = MediaFormat.MIMETYPE_VIDEO_AVC;
    private static final int BITRATE_MODE_CBR = 2;
    private static final int VIDEO_ENCODER_BIT_RATE = 16000000;
    private static final int VIDEO_ENCODER_FRAME_RATE = 30;
    private static final int VIDEO_ENCODER_I_FRAME_INTERVAL = 2;
    private static final int VIDEO_ENCODER_COLOR_FORMAT = CodecCapabilities.COLOR_Format24bitBGR888;

    private static final String AUDIO_DECODER_MIME = MediaFormat.MIMETYPE_AUDIO_OPUS;
    private static final int AUDIO_DECODER_SAMPLE_RATE = 48000;
    private static final int AUDIO_DECODER_CHANNEL_COUNT = 2;
    private static final byte[] OPUS_IDENTIFICATION_HEADER = "OpusHead".getBytes();
    private static final byte[] OPUS_PRE_SKIP_NSEC = ByteBuffer.allocate(8).putLong(11971).array();
    private static final byte[] OPUS_SEEK_PRE_ROLL_NSEC =
            ByteBuffer.allocate(8).putLong(80000000).array();

    private static class MockHdrMetadata extends HdrMetadata {
        public boolean was_called;

        @Override
        public void addMetadataToFormat(MediaFormat format) {
            was_called = true;
        }
    }

    @Test
    public void testCreateVideoDecoderWithNoCodecSpecificData() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        false,
                        VIDEO_PROFILE);
        assertFalse(format.containsKey("csd-0"));
        assertFalse(format.containsKey("csd-1"));
        assertFalse(format.containsKey("csd-2"));
    }

    @Test
    public void testCreateVideoDecoderWithSingleCodecSpecificDataBuffer() {
        byte[] csd0 = AVC_SPS_EXAMPLE;
        byte[][] csds = {csd0};
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        false,
                        VIDEO_PROFILE);
        assertEquals(format.getByteBuffer("csd-0"), ByteBuffer.wrap(csd0));
        assertFalse(format.containsKey("csd-1"));
        assertFalse(format.containsKey("csd-2"));
    }

    @Test
    public void testCreateVideoDecoderWithMultipleCodecSpecificDataBuffers() {
        byte[] csd0 = AVC_SPS_EXAMPLE;
        byte[] csd1 = AVC_PPS_EXAMPLE;
        byte[][] csds = {csd0, csd1};
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        false,
                        VIDEO_PROFILE);
        assertEquals(format.getByteBuffer("csd-0"), ByteBuffer.wrap(csd0));
        assertEquals(format.getByteBuffer("csd-1"), ByteBuffer.wrap(csd1));
        assertFalse(format.containsKey("csd-2"));
    }

    @Test
    public void testCreateVideoDecoderWithHdrMetadata() {
        byte[][] csds = {};
        MockHdrMetadata hdrMetadata = new MockHdrMetadata();
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        hdrMetadata,
                        false,
                        VIDEO_PROFILE);
        assertTrue(hdrMetadata.was_called);
    }

    @Test
    public void testCreateVideoDecoderWithAdaptivePlaybackDisabled() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        false,
                        VIDEO_PROFILE);
        assertFalse(format.containsKey(MediaFormat.KEY_MAX_WIDTH));
        assertFalse(format.containsKey(MediaFormat.KEY_MAX_HEIGHT));
    }

    @Test
    public void testCreateVideoDecoderWithAdaptivePlaybackEnabled() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        VIDEO_DECODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        true,
                        VIDEO_PROFILE);
        assertTrue(format.containsKey(MediaFormat.KEY_MAX_WIDTH));
        assertTrue(format.containsKey(MediaFormat.KEY_MAX_HEIGHT));
    }

    @Test
    public void testCreateDolbyVisionDecoderWithProfile() {
        String dvVideoDecoderMime = MediaFormat.MIMETYPE_VIDEO_DOLBY_VISION;
        int dvProfile5 = VideoCodecProfile.DOLBYVISION_PROFILE5;
        int dvProfile8 = VideoCodecProfile.DOLBYVISION_PROFILE8;
        byte[][] csds = {};

        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        dvVideoDecoderMime,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        true,
                        dvProfile5);
        assertEquals(
                format.getInteger(MediaFormat.KEY_PROFILE),
                MediaCodecInfo.CodecProfileLevel.DolbyVisionProfileDvheStn);

        format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        dvVideoDecoderMime,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        true,
                        dvProfile8);
        assertEquals(
                format.getInteger(MediaFormat.KEY_PROFILE),
                MediaCodecInfo.CodecProfileLevel.DolbyVisionProfileDvheSt);
    }

    @Test
    public void testDolbyVisionDecoderMaxInputSize() {
        byte[][] csds = {};

        // Estimate the maximum input size assuming three channel 4:2:0 subsampled input frames.
        int minCompressionRatio = 4;
        int expectedMaxInputSize = (VIDEO_WIDTH * VIDEO_HEIGHT * 3) / (2 * minCompressionRatio);
        MediaFormat format =
                MediaFormatBuilder.createVideoDecoderFormat(
                        MediaFormat.MIMETYPE_VIDEO_DOLBY_VISION,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        csds,
                        null,
                        true,
                        VideoCodecProfile.DOLBYVISION_PROFILE5);
        assertEquals(format.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE), expectedMaxInputSize);
    }

    @Test
    public void testCreateVideoEncoderSetsRelevantKeys() {
        MediaFormat format =
                MediaFormatBuilder.createVideoEncoderFormat(
                        VIDEO_ENCODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        BITRATE_MODE_CBR,
                        VIDEO_ENCODER_BIT_RATE,
                        VIDEO_ENCODER_FRAME_RATE,
                        VIDEO_ENCODER_I_FRAME_INTERVAL,
                        VIDEO_ENCODER_COLOR_FORMAT,
                        false);
        assertEquals(format.getInteger(MediaFormat.KEY_BIT_RATE), VIDEO_ENCODER_BIT_RATE);
        assertEquals(format.getInteger(MediaFormat.KEY_BITRATE_MODE), BITRATE_MODE_CBR);
        assertEquals(format.getInteger(MediaFormat.KEY_FRAME_RATE), VIDEO_ENCODER_FRAME_RATE);
        assertEquals(
                format.getInteger(MediaFormat.KEY_I_FRAME_INTERVAL),
                VIDEO_ENCODER_I_FRAME_INTERVAL);
        assertEquals(format.getInteger(MediaFormat.KEY_COLOR_FORMAT), VIDEO_ENCODER_COLOR_FORMAT);
    }

    @Test
    public void testCreateVideoEncoderWithAdaptivePlaybackDisabled() {
        MediaFormat format =
                MediaFormatBuilder.createVideoEncoderFormat(
                        VIDEO_ENCODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        BITRATE_MODE_CBR,
                        VIDEO_ENCODER_BIT_RATE,
                        VIDEO_ENCODER_FRAME_RATE,
                        VIDEO_ENCODER_I_FRAME_INTERVAL,
                        VIDEO_ENCODER_COLOR_FORMAT,
                        false);
        assertFalse(format.containsKey(MediaFormat.KEY_MAX_WIDTH));
        assertFalse(format.containsKey(MediaFormat.KEY_MAX_HEIGHT));
    }

    @Test
    public void testCreateVideoEncoderWithAdaptivePlaybackEnabled() {
        MediaFormat format =
                MediaFormatBuilder.createVideoEncoderFormat(
                        VIDEO_ENCODER_MIME,
                        VIDEO_WIDTH,
                        VIDEO_HEIGHT,
                        BITRATE_MODE_CBR,
                        VIDEO_ENCODER_BIT_RATE,
                        VIDEO_ENCODER_FRAME_RATE,
                        VIDEO_ENCODER_I_FRAME_INTERVAL,
                        VIDEO_ENCODER_COLOR_FORMAT,
                        true);
        assertTrue(format.containsKey(MediaFormat.KEY_MAX_WIDTH));
        assertTrue(format.containsKey(MediaFormat.KEY_MAX_HEIGHT));
    }

    @Test
    public void testCreateAudioFormatWithoutAdtsHeader() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createAudioFormat(
                        AUDIO_DECODER_MIME,
                        AUDIO_DECODER_SAMPLE_RATE,
                        AUDIO_DECODER_CHANNEL_COUNT,
                        csds,
                        false);
        assertFalse(format.containsKey(MediaFormat.KEY_IS_ADTS));
    }

    @Test
    public void testCreateAudioFormatWithAdtsHeader() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createAudioFormat(
                        AUDIO_DECODER_MIME,
                        AUDIO_DECODER_SAMPLE_RATE,
                        AUDIO_DECODER_CHANNEL_COUNT,
                        csds,
                        true);
        assertEquals(format.getInteger(MediaFormat.KEY_IS_ADTS), 1);
    }

    @Test
    public void testCreateAudioFormatWithoutCsds() {
        byte[][] csds = {};
        MediaFormat format =
                MediaFormatBuilder.createAudioFormat(
                        AUDIO_DECODER_MIME,
                        AUDIO_DECODER_SAMPLE_RATE,
                        AUDIO_DECODER_CHANNEL_COUNT,
                        csds,
                        false);
        assertFalse(format.containsKey("csd-0"));
        assertFalse(format.containsKey("csd-1"));
        assertFalse(format.containsKey("csd-2"));
    }

    @Test
    public void testCreateAudioFormatWithCsds() {
        byte[] csd0 = OPUS_IDENTIFICATION_HEADER;
        byte[] csd1 = OPUS_PRE_SKIP_NSEC;
        byte[] csd2 = OPUS_SEEK_PRE_ROLL_NSEC;
        byte[][] csds = {csd0, csd1, csd2};
        MediaFormat format =
                MediaFormatBuilder.createAudioFormat(
                        AUDIO_DECODER_MIME,
                        AUDIO_DECODER_SAMPLE_RATE,
                        AUDIO_DECODER_CHANNEL_COUNT,
                        csds,
                        false);
        assertEquals(format.getByteBuffer("csd-0"), ByteBuffer.wrap(csd0));
        assertEquals(format.getByteBuffer("csd-1"), ByteBuffer.wrap(csd1));
        assertEquals(format.getByteBuffer("csd-2"), ByteBuffer.wrap(csd2));
    }
}
