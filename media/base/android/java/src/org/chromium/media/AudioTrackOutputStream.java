// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.nio.ByteBuffer;

@JNINamespace("media")
class AudioTrackOutputStream {
    static class AudioBufferInfo {
        private final int mNumFrames;
        private final int mNumBytes;

        public AudioBufferInfo(int frames, int bytes) {
            mNumFrames = frames;
            mNumBytes = bytes;
        }

        public int getNumFrames() {
            return mNumFrames;
        }

        public int getNumBytes() {
            return mNumBytes;
        }
    }

    // Provide dependency injection points for unit tests.
    interface Callback {
        int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat);
        AudioTrack createAudioTrack(int streamType, int sampleRateInHz, int channelConfig,
                int audioFormat, int bufferSizeInBytes, int mode);
        AudioBufferInfo onMoreData(ByteBuffer audioData, long delayInFrames);
        long getAddress(ByteBuffer byteBuffer);
        void onError();
    }

    private static final String TAG = "AudioTrackOutput";
    // Must be the same as AudioBus::kChannelAlignment.
    private static final int CHANNEL_ALIGNMENT = 16;

    private long mNativeAudioTrackOutputStream;
    private Callback mCallback;
    private AudioTrack mAudioTrack;
    private int mBufferSizeInBytes;
    private WorkerThread mWorkerThread;

    // See
    // https://developer.android.com/reference/android/media/AudioTrack.html#getPlaybackHeadPosition().
    // Though the "int" type is signed 32-bits, |mLastPlaybackHeadPosition| should be reinterpreted
    // as if it is unsigned 32-bits. It will wrap (overflow) periodically, for example approximately
    // once every 27:03:11 hours:minutes:seconds at 44.1 kHz.
    private int mLastPlaybackHeadPosition;
    private long mTotalPlayedFrames;
    private long mTotalReadFrames;

    private ByteBuffer mReadBuffer;
    private ByteBuffer mWriteBuffer;
    private int mLeftSize;

    class WorkerThread extends Thread {
        private volatile boolean mDone;

        public void finish() {
            mDone = true;
        }

        @Override
        public void run() {
            // This should not be a busy loop, since the thread would be blocked in either
            // AudioSyncReader::WaitUntilDataIsReady() or AudioTrack.write().
            while (!mDone) {
                int left = writeData();
                // AudioTrack.write() failed, exit the run loop.
                if (left < 0) break;
                // Only partial data is written, retry again.
                if (left > 0) continue;

                readMoreData();
            }
        }
    }

    @CalledByNative
    private static AudioTrackOutputStream create() {
        return new AudioTrackOutputStream(null);
    }

    @VisibleForTesting
    static AudioTrackOutputStream create(Callback callback) {
        return new AudioTrackOutputStream(callback);
    }

    private AudioTrackOutputStream(Callback callback) {
        mCallback = callback;
        if (mCallback != null) return;

        mCallback = new Callback() {
            @Override
            public int getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat) {
                return AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat);
            }

            @Override
            public AudioTrack createAudioTrack(int streamType, int sampleRateInHz,
                    int channelConfig, int audioFormat, int bufferSizeInBytes, int mode) {
                return new AudioTrack(streamType, sampleRateInHz, channelConfig, audioFormat,
                        bufferSizeInBytes, mode);
            }

            @Override
            public AudioBufferInfo onMoreData(ByteBuffer audioData, long delayInFrames) {
                return AudioTrackOutputStreamJni.get().onMoreData(mNativeAudioTrackOutputStream,
                        AudioTrackOutputStream.this, audioData, delayInFrames);
            }

            @Override
            public long getAddress(ByteBuffer byteBuffer) {
                return AudioTrackOutputStreamJni.get().getAddress(
                        mNativeAudioTrackOutputStream, AudioTrackOutputStream.this, byteBuffer);
            }

            @Override
            public void onError() {
                AudioTrackOutputStreamJni.get().onError(
                        mNativeAudioTrackOutputStream, AudioTrackOutputStream.this);
            }
        };
    }

    @SuppressWarnings("deprecation")
    private int getChannelConfig(int channelCount) {
        switch (channelCount) {
            case 1:
                return AudioFormat.CHANNEL_OUT_MONO;
            case 2:
                return AudioFormat.CHANNEL_OUT_STEREO;
            case 4:
                return AudioFormat.CHANNEL_OUT_QUAD;
            case 6:
                return AudioFormat.CHANNEL_OUT_5POINT1;
            case 8:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
                } else {
                    return AudioFormat.CHANNEL_OUT_7POINT1;
                }
            default:
                return AudioFormat.CHANNEL_OUT_DEFAULT;
        }
    }

    @CalledByNative
    boolean open(int channelCount, int sampleRate, int sampleFormat) {
        assert mAudioTrack == null;

        int channelConfig = getChannelConfig(channelCount);
        // Use 3x buffers here to avoid momentary underflow from the renderer.
        mBufferSizeInBytes =
                3 * mCallback.getMinBufferSize(sampleRate, channelConfig, sampleFormat);

        try {
            Log.d(TAG, "Crate AudioTrack with sample rate:%d, channel:%d, format:%d ", sampleRate,
                    channelConfig, sampleFormat);

            mAudioTrack = mCallback.createAudioTrack(AudioManager.STREAM_MUSIC, sampleRate,
                    channelConfig, sampleFormat, mBufferSizeInBytes, AudioTrack.MODE_STREAM);
            assert mAudioTrack != null;
        } catch (IllegalArgumentException ile) {
            Log.e(TAG, "Exception creating AudioTrack for playback: ", ile);
            return false;
        }

        // AudioTrack would be in UNINITIALIZED state if we give unsupported configurations. For
        // example, create an AC3 bitstream AudioTrack when the connected audio sink does not
        // support AC3.
        // https://developer.android.com/reference/android/media/AudioTrack.html#STATE_UNINITIALIZED
        if (mAudioTrack.getState() == AudioTrack.STATE_UNINITIALIZED) {
            Log.e(TAG, "Cannot create AudioTrack");
            mAudioTrack = null;
            return false;
        }

        mLastPlaybackHeadPosition = 0;
        mTotalPlayedFrames = 0;
        return true;
    }

    private ByteBuffer allocateAlignedByteBuffer(int capacity, int alignment) {
        int mask = alignment - 1;
        ByteBuffer buffer = ByteBuffer.allocateDirect(capacity + mask);
        long address = mCallback.getAddress(buffer);

        int offset = (alignment - (int) (address & mask)) & mask;
        buffer.position(offset);
        buffer.limit(offset + capacity);
        return buffer.slice();
    }

    @CalledByNative
    void start(long nativeAudioTrackOutputStream) {
        Log.d(TAG, "AudioTrackOutputStream.start()");
        if (mWorkerThread != null) return;

        mNativeAudioTrackOutputStream = nativeAudioTrackOutputStream;
        mTotalReadFrames = 0;
        mReadBuffer = allocateAlignedByteBuffer(mBufferSizeInBytes, CHANNEL_ALIGNMENT);

        mAudioTrack.play();

        mWorkerThread = new WorkerThread();
        mWorkerThread.start();
    }

    @CalledByNative
    void stop() {
        Log.d(TAG, "AudioTrackOutputStream.stop()");
        if (mWorkerThread != null) {
            mWorkerThread.finish();
            try {
                mWorkerThread.interrupt();
                mWorkerThread.join();
            } catch (SecurityException e) {
                Log.e(TAG, "Exception while waiting for AudioTrack worker thread finished: ", e);
            } catch (InterruptedException e) {
                Log.e(TAG, "Exception while waiting for AudioTrack worker thread finished: ", e);
            }
            mWorkerThread = null;
        }

        mAudioTrack.pause();
        mAudioTrack.flush();
        mLastPlaybackHeadPosition = 0;
        mTotalPlayedFrames = 0;
        mNativeAudioTrackOutputStream = 0;
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    void setVolume(double volume) {
        // Chrome sends the volume in the range [0, 1.0], whereas Android
        // expects the volume to be within [0, getMaxVolume()].
        float scaledVolume = (float) (volume * AudioTrack.getMaxVolume());
        mAudioTrack.setStereoVolume(scaledVolume, scaledVolume);
    }

    @CalledByNative
    void close() {
        Log.d(TAG, "AudioTrackOutputStream.close()");
        if (mAudioTrack != null) {
            mAudioTrack.release();
            mAudioTrack = null;
        }
    }

    @CalledByNative
    AudioBufferInfo createAudioBufferInfo(int frames, int size) {
        return new AudioBufferInfo(frames, size);
    }

    private void readMoreData() {
        assert mNativeAudioTrackOutputStream != 0;

        // Although the return value of AudioTrack.getPlaybackHeadPosition() should be unsigned
        // 32-bit integer and would overflow, it is correct to calculate the difference between
        // two continuous callings of AudioTrack.getPlaybackHeadPosition() as long as the
        // real difference is less than 0x7FFFFFFF.
        int position = mAudioTrack.getPlaybackHeadPosition();
        mTotalPlayedFrames += position - mLastPlaybackHeadPosition;
        mLastPlaybackHeadPosition = position;

        long delayInFrames = mTotalReadFrames - mTotalPlayedFrames;
        if (delayInFrames < 0) delayInFrames = 0;

        AudioBufferInfo info = mCallback.onMoreData(mReadBuffer.duplicate(), delayInFrames);
        if (info == null || info.getNumBytes() <= 0) return;

        mTotalReadFrames += info.getNumFrames();

        mWriteBuffer = mReadBuffer.asReadOnlyBuffer();
        mLeftSize = info.getNumBytes();
    }

    private int writeData() {
        if (mLeftSize == 0) return 0;

        int written = writeAudioTrack();
        if (written < 0) {
            Log.e(TAG, "AudioTrack.write() failed. Error:" + written);
            mCallback.onError();
            return written;
        }

        assert mLeftSize >= written;
        mLeftSize -= written;

        return mLeftSize;
    }

    @SuppressLint("NewApi")
    private int writeAudioTrack() {
        // This class is used for compressed audio bitstream playback, which is supported since
        // Android L, so it should be fine to use level 21 APIs directly.
        return mAudioTrack.write(mWriteBuffer, mLeftSize, AudioTrack.WRITE_BLOCKING);
    }

    @NativeMethods
    interface Natives {
        AudioBufferInfo onMoreData(long nativeAudioTrackOutputStream, AudioTrackOutputStream caller,
                ByteBuffer audioData, long delayInFrames);
        void onError(long nativeAudioTrackOutputStream, AudioTrackOutputStream caller);
        long getAddress(long nativeAudioTrackOutputStream, AudioTrackOutputStream caller,
                ByteBuffer byteBuffer);
    }
}
