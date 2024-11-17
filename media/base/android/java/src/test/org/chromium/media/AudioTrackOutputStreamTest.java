// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAudioTrack;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.media.AudioTrackOutputStream.AudioBufferInfo;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Tests for AudioTrackOutputStream. */
@RunWith(BaseRobolectricTestRunner.class)
// Need sdk > Q for robolectric 4.6.
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.Q)
public class AudioTrackOutputStreamTest {
    static class ObservableAudioTrack extends AudioTrack {
        private List<Byte> mReceivedData = new ArrayList<Byte>();
        private boolean mPartialWrite = true;

        public ObservableAudioTrack(
                int streamType,
                int sampleRateInHz,
                int channelConfig,
                int audioFormat,
                int bufferSizeInBytes,
                int mode) {
            super(streamType, sampleRateInHz, channelConfig, audioFormat, bufferSizeInBytes, mode);
        }

        @Override
        public int write(ByteBuffer audioData, int sizeInBytes, int writeMode) {
            int writternSize = mPartialWrite ? sizeInBytes / 2 : sizeInBytes;
            mPartialWrite = !mPartialWrite;

            if (writternSize > 0) {
                byte[] array = new byte[writternSize];
                audioData.get(array);
                recordData(array, 0, writternSize);
            }

            return writternSize;
        }

        @Override
        public int write(byte[] audioData, int offsetInBytes, int sizeInBytes) {
            int writternSize = mPartialWrite ? sizeInBytes / 2 : sizeInBytes;
            mPartialWrite = !mPartialWrite;

            if (writternSize > 0) recordData(audioData, offsetInBytes, writternSize);

            return writternSize;
        }

        private void recordData(byte[] audioData, int offsetInBytes, int sizeInBytes) {
            for (; sizeInBytes > 0; --sizeInBytes) mReceivedData.add(audioData[offsetInBytes++]);
        }

        public List<Byte> getReceivedData() {
            return mReceivedData;
        }
    }

    static class DataProvider implements AudioTrackOutputStream.Callback {
        private static final int MIN_BUFFER_SIZE = 800;
        private List<Byte> mGeneratedData = new ArrayList<Byte>();
        private CountDownLatch mDoneSignal;
        private ObservableAudioTrack mAudioTrack;

        public DataProvider(int bufferCount) {
            assert bufferCount > 0;
            mDoneSignal = new CountDownLatch(bufferCount + 1);
        }

        public void updateBufferCount(int bufferCount) {
            assert bufferCount > 0;
            mDoneSignal = new CountDownLatch(bufferCount + 1);
        }

        @Override
        public int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat) {
            return MIN_BUFFER_SIZE;
        }

        @Override
        public AudioTrack createAudioTrack(
                int streamType,
                int sampleRateInHz,
                int channelConfig,
                int audioFormat,
                int bufferSizeInBytes,
                int mode) {
            mAudioTrack =
                    new ObservableAudioTrack(
                            streamType,
                            sampleRateInHz,
                            channelConfig,
                            audioFormat,
                            bufferSizeInBytes,
                            mode);
            return mAudioTrack;
        }

        @Override
        public AudioBufferInfo onMoreData(ByteBuffer audioData, long delayInFrames) {
            mDoneSignal.countDown();
            if (mDoneSignal.getCount() <= 0) {
                try {
                    Thread.sleep(3);
                } catch (Exception e) {
                }
                return null;
            }

            final int dataSize = MIN_BUFFER_SIZE;
            for (int i = 0; i < dataSize; ++i) {
                byte data = (byte) i;
                audioData.put(data);
                mGeneratedData.add(data);
            }
            return new AudioBufferInfo(dataSize, dataSize);
        }

        @Override
        public long getAddress(ByteBuffer byteBuffer) {
            return 0x10001L;
        }

        @Override
        public void onError() {}

        public void waitForOutOfData() throws InterruptedException {
            mDoneSignal.await(300, TimeUnit.MILLISECONDS);
        }

        public List<Byte> getGeneratedData() {
            return mGeneratedData;
        }

        public List<Byte> getReceivedData() {
            return mAudioTrack.getReceivedData();
        }

        public int getBufferSize() {
            return MIN_BUFFER_SIZE;
        }
    }

    @Before
    public void setUp() {
        ShadowAudioTrack.addAllowedNonPcmEncoding(AudioFormat.ENCODING_E_AC3);
    }

    @Test
    public void playSimpleBitstream() throws InterruptedException {
        DataProvider provider = new DataProvider(3);

        AudioTrackOutputStream stream = AudioTrackOutputStream.create(provider);
        stream.open(2, 44100, AudioFormat.ENCODING_E_AC3);
        stream.start(0x888);

        provider.waitForOutOfData();
        List<Byte> generatedData = provider.getGeneratedData();
        List<Byte> receivedData = provider.getReceivedData();

        assertEquals(3L * provider.getBufferSize(), generatedData.size());
        assertArrayEquals(generatedData.toArray(), receivedData.toArray());

        stream.stop();
        stream.close();
    }

    @Test
    public void playPiecewiseBitstream() throws InterruptedException {
        DataProvider provider = new DataProvider(3);

        AudioTrackOutputStream stream = AudioTrackOutputStream.create(provider);
        stream.open(2, 44100, AudioFormat.ENCODING_E_AC3);
        stream.start(0x888);

        provider.waitForOutOfData();

        provider.updateBufferCount(3);
        provider.waitForOutOfData();

        List<Byte> generatedData = provider.getGeneratedData();
        List<Byte> receivedData = provider.getReceivedData();

        assertTrue(6 * provider.getBufferSize() <= generatedData.size());
        assertArrayEquals(generatedData.toArray(), receivedData.toArray());

        stream.stop();
        stream.close();
    }
}
