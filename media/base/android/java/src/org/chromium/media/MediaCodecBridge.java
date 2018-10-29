// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.Queue;

/**
 * A MediaCodec wrapper for adapting the API and catching exceptions.
 */
@JNINamespace("media")
class MediaCodecBridge {
    private static final String TAG = "cr_MediaCodecBridge";

    // After a flush(), dequeueOutputBuffer() can often produce empty presentation timestamps
    // for several frames. As a result, the player may find that the time does not increase
    // after decoding a frame. To detect this, we check whether the presentation timestamp from
    // dequeueOutputBuffer() is larger than input_timestamp - MAX_PRESENTATION_TIMESTAMP_SHIFT_US
    // after a flush. And we set the presentation timestamp from dequeueOutputBuffer() to be
    // non-decreasing for the remaining frames.
    private static final long MAX_PRESENTATION_TIMESTAMP_SHIFT_US = 100000;

    // We use only one output audio format (PCM16) that has 2 bytes per sample
    private static final int PCM16_BYTES_PER_SAMPLE = 2;

    private static final int MEDIA_CODEC_UNKNOWN_CIPHER_MODE = -1;

    // TODO(qinmin): Use MediaFormat constants when part of the public API.
    private static final String KEY_CROP_LEFT = "crop-left";
    private static final String KEY_CROP_RIGHT = "crop-right";
    private static final String KEY_CROP_BOTTOM = "crop-bottom";
    private static final String KEY_CROP_TOP = "crop-top";

    protected MediaCodec mMediaCodec;

    private ByteBuffer[] mInputBuffers;
    private ByteBuffer[] mOutputBuffers;

    private boolean mFlushed;
    private long mLastPresentationTimeUs;
    private BitrateAdjuster mBitrateAdjuster;

    // To support both the synchronous and asynchronous version of MediaCodec
    // (since we need to work on <M devices), we implement async support as a
    // layer under synchronous API calls and provide a callback signal for when
    // work (new input, new output, errors, or format changes) is available.
    //
    // Once the callback has been set on MediaCodec, these variables must only
    // be accessed from synchronized(this) blocks since MediaCodecCallback may
    // execute on an arbitrary thread.
    private boolean mUseAsyncApi;
    private Queue<GetOutputFormatResult> mPendingFormat;
    private GetOutputFormatResult mCurrentFormat;
    private boolean mPendingError;
    private boolean mPendingStart;
    private long mNativeMediaCodecBridge;
    private int mSequenceCounter;
    private Queue<DequeueInputResult> mPendingInputBuffers;
    private Queue<DequeueOutputResult> mPendingOutputBuffers;

    // Set by tests which don't have a Java MessagePump to ensure the MediaCodec
    // callbacks are actually delivered. Always null in production.
    private static HandlerThread sCallbackHandlerThread;
    private static Handler sCallbackHandler;

    @MainDex
    private static class DequeueInputResult {
        private final int mStatus;
        private final int mIndex;

        private DequeueInputResult(int status, int index) {
            mStatus = status;
            mIndex = index;
        }

        @CalledByNative("DequeueInputResult")
        private int status() {
            return mStatus;
        }

        @CalledByNative("DequeueInputResult")
        private int index() {
            return mIndex;
        }
    }

    @MainDex
    private static class DequeueOutputResult {
        private final int mStatus;
        private final int mIndex;
        private final int mFlags;
        private final int mOffset;
        private final long mPresentationTimeMicroseconds;
        private final int mNumBytes;

        private DequeueOutputResult(int status, int index, int flags, int offset,
                long presentationTimeMicroseconds, int numBytes) {
            mStatus = status;
            mIndex = index;
            mFlags = flags;
            mOffset = offset;
            mPresentationTimeMicroseconds = presentationTimeMicroseconds;
            mNumBytes = numBytes;
        }

        @CalledByNative("DequeueOutputResult")
        private int status() {
            return mStatus;
        }

        @CalledByNative("DequeueOutputResult")
        private int index() {
            return mIndex;
        }

        @CalledByNative("DequeueOutputResult")
        private int flags() {
            return mFlags;
        }

        @CalledByNative("DequeueOutputResult")
        private int offset() {
            return mOffset;
        }

        @CalledByNative("DequeueOutputResult")
        private long presentationTimeMicroseconds() {
            return mPresentationTimeMicroseconds;
        }

        @CalledByNative("DequeueOutputResult")
        private int numBytes() {
            return mNumBytes;
        }
    }

    /** A wrapper around a MediaFormat. */
    @MainDex
    private static class GetOutputFormatResult {
        private final int mStatus;
        // May be null if mStatus is not MediaCodecStatus.OK.
        private final MediaFormat mFormat;

        private GetOutputFormatResult(int status, MediaFormat format) {
            mStatus = status;
            mFormat = format;
        }

        private boolean formatHasCropValues() {
            return mFormat.containsKey(KEY_CROP_RIGHT) && mFormat.containsKey(KEY_CROP_LEFT)
                    && mFormat.containsKey(KEY_CROP_BOTTOM) && mFormat.containsKey(KEY_CROP_TOP);
        }

        @CalledByNative("GetOutputFormatResult")
        private int status() {
            return mStatus;
        }

        @CalledByNative("GetOutputFormatResult")
        private int width() {
            return formatHasCropValues()
                    ? mFormat.getInteger(KEY_CROP_RIGHT) - mFormat.getInteger(KEY_CROP_LEFT) + 1
                    : mFormat.getInteger(MediaFormat.KEY_WIDTH);
        }

        @CalledByNative("GetOutputFormatResult")
        private int height() {
            return formatHasCropValues()
                    ? mFormat.getInteger(KEY_CROP_BOTTOM) - mFormat.getInteger(KEY_CROP_TOP) + 1
                    : mFormat.getInteger(MediaFormat.KEY_HEIGHT);
        }

        @CalledByNative("GetOutputFormatResult")
        private int sampleRate() {
            return mFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
        }

        @CalledByNative("GetOutputFormatResult")
        private int channelCount() {
            return mFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
        }
    }

    // Warning: This class may execute on an arbitrary thread for the lifetime
    // of the MediaCodec. The MediaCodecBridge methods it calls are synchronized
    // to avoid race conditions.
    @MainDex
    @TargetApi(Build.VERSION_CODES.M)
    class MediaCodecCallback extends MediaCodec.Callback {
        private MediaCodecBridge mMediaCodecBridge;
        MediaCodecCallback(MediaCodecBridge bridge) {
            mMediaCodecBridge = bridge;
        }

        @Override
        public void onError(MediaCodec codec, MediaCodec.CodecException e) {
            // TODO(dalecurtis): We may want to drop transient errors here.
            Log.e(TAG, "MediaCodec.onError: " + e.getDiagnosticInfo());
            mMediaCodecBridge.onError(e);
        }

        @Override
        public void onInputBufferAvailable(MediaCodec codec, int index) {
            mMediaCodecBridge.onInputBufferAvailable(index);
        }

        @Override
        public void onOutputBufferAvailable(
                MediaCodec codec, int index, MediaCodec.BufferInfo info) {
            mMediaCodecBridge.onOutputBufferAvailable(index, info);
        }

        @Override
        public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            mMediaCodecBridge.onOutputFormatChanged(format);
        }
    };

    MediaCodecBridge(MediaCodec mediaCodec, BitrateAdjuster bitrateAdjuster, boolean useAsyncApi) {
        assert mediaCodec != null;
        mMediaCodec = mediaCodec;
        mLastPresentationTimeUs = 0;
        mFlushed = true;
        mBitrateAdjuster = bitrateAdjuster;
        mUseAsyncApi = useAsyncApi;

        if (!mUseAsyncApi) return;

        enableAsyncApi();
        prepareAsyncApiForRestart();
    }

    // There's a Lollipop version of the setCallback() API, so we could enable
    // it there, but since it's likely to be more stable in later SDK versions
    // and our tests require their own Handler to pump the callbacks, we limit
    // support to Marshmallow only.
    @TargetApi(Build.VERSION_CODES.M)
    private void enableAsyncApi() {
        mPendingError = false;
        mPendingFormat = new LinkedList<GetOutputFormatResult>();
        mPendingInputBuffers = new LinkedList<DequeueInputResult>();
        mPendingOutputBuffers = new LinkedList<DequeueOutputResult>();
        mMediaCodec.setCallback(new MediaCodecCallback(this), sCallbackHandler);
    }

    // The methods below are all synchronized because we may receive callbacks
    // from the MediaCodecCallback on a different thread; especially in the
    // testing case where we create a separate HandlerThread.

    private synchronized void prepareAsyncApiForRestart() {
        mPendingFormat.clear();
        mPendingInputBuffers.clear();
        mPendingOutputBuffers.clear();
        mPendingStart = true;
        mCurrentFormat = null;
        ++mSequenceCounter;
    }

    @CalledByNative
    private synchronized void setBuffersAvailableListener(long nativeMediaCodecBridge) {
        mNativeMediaCodecBridge = nativeMediaCodecBridge;

        // If any buffers or errors occurred before this, trigger the callback now.
        if (!mPendingInputBuffers.isEmpty() || !mPendingOutputBuffers.isEmpty() || mPendingError)
            notifyBuffersAvailable();
    }

    private synchronized void notifyBuffersAvailable() {
        if (mNativeMediaCodecBridge != 0) nativeOnBuffersAvailable(mNativeMediaCodecBridge);
    }

    public synchronized void onError(MediaCodec.CodecException e) {
        mPendingError = true;
        mPendingInputBuffers.clear();
        mPendingOutputBuffers.clear();
        notifyBuffersAvailable();
    }

    public synchronized void onInputBufferAvailable(int index) {
        if (mPendingStart) return;

        mPendingInputBuffers.add(new DequeueInputResult(MediaCodecStatus.OK, index));
        notifyBuffersAvailable();
    }

    public synchronized void onOutputBufferAvailable(int index, MediaCodec.BufferInfo info) {
        // Drop buffers that come in during a flush.
        if (mPendingStart) return;

        updateLastPresentationTime(info);
        mPendingOutputBuffers.add(new DequeueOutputResult(MediaCodecStatus.OK, index, info.flags,
                info.offset, info.presentationTimeUs, info.size));
        notifyBuffersAvailable();
    }

    public synchronized void onOutputFormatChanged(MediaFormat format) {
        mPendingOutputBuffers.add(
                new DequeueOutputResult(MediaCodecStatus.OUTPUT_FORMAT_CHANGED, -1, 0, 0, 0, 0));
        mPendingFormat.add(new GetOutputFormatResult(MediaCodecStatus.OK, format));
        notifyBuffersAvailable();
    }

    public synchronized void onPendingStartComplete(int sequenceCounter) {
        // Ignore events from the past.
        if (mSequenceCounter != sequenceCounter) return;
        mPendingStart = false;
    }

    void updateLastPresentationTime(MediaCodec.BufferInfo info) {
        if (info.presentationTimeUs < mLastPresentationTimeUs) {
            // TODO(qinmin): return a special code through DequeueOutputResult
            // to notify the native code the the frame has a wrong presentation
            // timestamp and should be skipped.
            info.presentationTimeUs = mLastPresentationTimeUs;
        }
        mLastPresentationTimeUs = info.presentationTimeUs;
    }

    @CalledByNative
    void release() {
        if (mUseAsyncApi) {
            // Disconnect from the native code to ensure we don't issue calls
            // into it after its destruction.
            synchronized (this) {
                mNativeMediaCodecBridge = 0;
            }
        }
        try {
            String codecName = "unknown";
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                codecName = mMediaCodec.getName();
            }
            // This logging is to help us identify hung MediaCodecs in crash reports.
            Log.w(TAG, "Releasing: " + codecName);
            mMediaCodec.release();
            Log.w(TAG, "Codec released");
        } catch (IllegalStateException e) {
            // The MediaCodec is stuck in a bad state, possibly due to losing
            // the surface.
            Log.e(TAG, "Cannot release media codec", e);
        }
        mMediaCodec = null;
    }

    // TODO(sanfin): Move this to constructor or builder.
    @SuppressWarnings("deprecation")
    boolean start() {
        try {
            if (mUseAsyncApi) {
                synchronized (this) {
                    if (mPendingError) return false;

                    class CompletePendingStartTask implements Runnable {
                        private int mThisSequence;
                        CompletePendingStartTask(int sequence) {
                            mThisSequence = sequence;
                        }

                        @Override
                        public void run() {
                            onPendingStartComplete(mThisSequence);
                        }
                    };

                    // Ensure any pending indices are ignored until after start
                    // by trampolining through the handler/looper that the
                    // notifications are coming from.
                    Handler h = sCallbackHandler == null ? new Handler(Looper.getMainLooper())
                                                         : sCallbackHandler;
                    h.post(new CompletePendingStartTask(mSequenceCounter));
                }
            }

            mMediaCodec.start();
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.KITKAT) {
                mInputBuffers = mMediaCodec.getInputBuffers();
                mOutputBuffers = mMediaCodec.getOutputBuffers();
            }
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            return false;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            return false;
        }
        return true;
    }

    @CalledByNative
    private DequeueInputResult dequeueInputBuffer(long timeoutUs) {
        if (mUseAsyncApi) {
            synchronized (this) {
                if (mPendingError) return new DequeueInputResult(MediaCodecStatus.ERROR, -1);
                if (mPendingStart || mPendingInputBuffers.isEmpty())
                    return new DequeueInputResult(MediaCodecStatus.TRY_AGAIN_LATER, -1);
                return mPendingInputBuffers.remove();
            }
        }

        int status = MediaCodecStatus.ERROR;
        int index = -1;
        try {
            int indexOrStatus = mMediaCodec.dequeueInputBuffer(timeoutUs);
            if (indexOrStatus >= 0) { // index!
                status = MediaCodecStatus.OK;
                index = indexOrStatus;
            } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                status = MediaCodecStatus.TRY_AGAIN_LATER;
            } else {
                Log.e(TAG, "Unexpected index_or_status: " + indexOrStatus);
                assert false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to dequeue input buffer", e);
        }
        return new DequeueInputResult(status, index);
    }

    @CalledByNative
    private int flush() {
        try {
            mFlushed = true;
            mMediaCodec.flush();

            // MediaCodec.flush() invalidates all returned indices, but there
            // may be some unhandled callbacks when using the async API. When
            // we call prepareAsyncApiForRestart() it will set mPendingStart,
            // start() will then post a task through the callback handler which
            // clears mPendingStart to start accepting new buffers.
            if (mUseAsyncApi) {
                prepareAsyncApiForRestart();
                if (!start()) return MediaCodecStatus.ERROR;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to flush MediaCodec", e);
            return MediaCodecStatus.ERROR;
        }
        return MediaCodecStatus.OK;
    }

    @CalledByNative
    private void stop() {
        try {
            mMediaCodec.stop();

            // MediaCodec.stop() invalidates all returned indices.
            if (mUseAsyncApi) prepareAsyncApiForRestart();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to stop MediaCodec", e);
        }
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @CalledByNative
    private String getName() {
        String codecName = "unknown";
        try {
            codecName = mMediaCodec.getName();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot get codec name", e);
        }
        return codecName;
    }

    @CalledByNative
    private GetOutputFormatResult getOutputFormat() {
        if (mUseAsyncApi && mCurrentFormat != null) return mCurrentFormat;

        MediaFormat format = null;
        int status = MediaCodecStatus.OK;
        try {
            format = mMediaCodec.getOutputFormat();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to get output format", e);
            status = MediaCodecStatus.ERROR;
        }
        return new GetOutputFormatResult(status, format);
    }

    /** Returns null if MediaCodec throws IllegalStateException. */
    @SuppressLint("NewApi")
    @CalledByNative
    private ByteBuffer getInputBuffer(int index) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            try {
                return mMediaCodec.getInputBuffer(index);
            } catch (IllegalStateException e) {
                Log.e(TAG, "Failed to get input buffer", e);
                return null;
            }
        }
        return mInputBuffers[index];
    }

    /** Returns null if MediaCodec throws IllegalStateException. */
    @SuppressLint("NewApi")
    @CalledByNative
    protected ByteBuffer getOutputBuffer(int index) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            try {
                return mMediaCodec.getOutputBuffer(index);
            } catch (IllegalStateException e) {
                Log.e(TAG, "Failed to get output buffer", e);
                return null;
            }
        }
        return mOutputBuffers[index];
    }

    @CalledByNative
    private int queueInputBuffer(
            int index, int offset, int size, long presentationTimeUs, int flags) {
        resetLastPresentationTimeIfNeeded(presentationTimeUs);
        try {
            mMediaCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
        } catch (Exception e) {
            Log.e(TAG, "Failed to queue input buffer", e);
            return MediaCodecStatus.ERROR;
        }
        return MediaCodecStatus.OK;
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @CalledByNative
    private void setVideoBitrate(int bps, int frameRate) {
        int targetBps = mBitrateAdjuster.getTargetBitrate(bps, frameRate);
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, targetBps);
        try {
            mMediaCodec.setParameters(b);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to set MediaCodec parameters", e);
        }
        Log.v(TAG,
                "setVideoBitrate: input " + bps + "bps@" + frameRate + ", targetBps " + targetBps);
    }

    @TargetApi(Build.VERSION_CODES.KITKAT)
    @CalledByNative
    private void requestKeyFrameSoon() {
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        try {
            mMediaCodec.setParameters(b);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to set MediaCodec parameters", e);
        }
    }

    // Incoming |native| values are as defined in media/base/encryption_scheme.h. Translated values
    // are from MediaCodec. At present, these values are in sync. Returns
    // MEDIA_CODEC_UNKNOWN_CIPHER_MODE in the case of unknown incoming value.
    private int translateCipherModeValue(int nativeValue) {
        switch (nativeValue) {
            case CipherMode.UNENCRYPTED:
                return MediaCodec.CRYPTO_MODE_UNENCRYPTED;
            case CipherMode.AES_CTR:
                return MediaCodec.CRYPTO_MODE_AES_CTR;
            case CipherMode.AES_CBC:
                return MediaCodec.CRYPTO_MODE_AES_CBC;
            default:
                Log.e(TAG, "Unsupported cipher mode: " + nativeValue);
                return MEDIA_CODEC_UNKNOWN_CIPHER_MODE;
        }
    }

    @SuppressLint("WrongConstant") // False positive on logging statement.
    @CalledByNative
    private int queueSecureInputBuffer(int index, int offset, byte[] iv, byte[] keyId,
            int[] numBytesOfClearData, int[] numBytesOfEncryptedData, int numSubSamples,
            int cipherMode, int patternEncrypt, int patternSkip, long presentationTimeUs) {
        resetLastPresentationTimeIfNeeded(presentationTimeUs);
        try {
            cipherMode = translateCipherModeValue(cipherMode);
            if (cipherMode == MEDIA_CODEC_UNKNOWN_CIPHER_MODE) {
                return MediaCodecStatus.ERROR;
            }
            boolean usesCbcs = cipherMode == MediaCodec.CRYPTO_MODE_AES_CBC;
            if (usesCbcs && !MediaCodecUtil.platformSupportsCbcsEncryption(Build.VERSION.SDK_INT)) {
                Log.e(TAG, "Encryption scheme 'cbcs' not supported on this platform.");
                return MediaCodecStatus.ERROR;
            }
            CryptoInfo cryptoInfo = new CryptoInfo();
            cryptoInfo.set(numSubSamples, numBytesOfClearData, numBytesOfEncryptedData, keyId, iv,
                    cipherMode);
            if (patternEncrypt != 0 && patternSkip != 0) {
                if (usesCbcs) {
                    // Above platform check ensured that setting the pattern is indeed supported.
                    MediaCodecUtil.setPatternIfSupported(cryptoInfo, patternEncrypt, patternSkip);
                } else {
                    Log.e(TAG, "Pattern encryption only supported for 'cbcs' scheme (CBC mode).");
                    return MediaCodecStatus.ERROR;
                }
            }
            mMediaCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
        } catch (MediaCodec.CryptoException e) {
            if (e.getErrorCode() == MediaCodec.CryptoException.ERROR_NO_KEY) {
                Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
                return MediaCodecStatus.NO_KEY;
            }
            Log.e(TAG,
                    "Failed to queue secure input buffer, CryptoException with error code "
                            + e.getErrorCode());
            return MediaCodecStatus.ERROR;
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException can occur when release() is called on the MediaCrypto
            // object, but the MediaCodecBridge is unaware of the change.
            Log.e(TAG, "Failed to queue secure input buffer, IllegalArgumentException " + e);
            return MediaCodecStatus.ERROR;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to queue secure input buffer, IllegalStateException " + e);
            return MediaCodecStatus.ERROR;
        }
        return MediaCodecStatus.OK;
    }

    @CalledByNative
    protected void releaseOutputBuffer(int index, boolean render) {
        try {
            mMediaCodec.releaseOutputBuffer(index, render);
        } catch (IllegalStateException e) {
            // TODO(qinmin): May need to report the error to the caller. crbug.com/356498.
            Log.e(TAG, "Failed to release output buffer", e);
        }
    }

    @SuppressWarnings("deprecation")
    @CalledByNative
    private DequeueOutputResult dequeueOutputBuffer(long timeoutUs) {
        if (mUseAsyncApi) {
            synchronized (this) {
                if (mPendingError)
                    return new DequeueOutputResult(MediaCodecStatus.ERROR, -1, 0, 0, 0, 0);
                if (mPendingOutputBuffers.isEmpty()) {
                    return new DequeueOutputResult(
                            MediaCodecStatus.TRY_AGAIN_LATER, -1, 0, 0, 0, 0);
                }
                if (mPendingOutputBuffers.peek().status()
                        == MediaCodecStatus.OUTPUT_FORMAT_CHANGED) {
                    assert !mPendingFormat.isEmpty();
                    mCurrentFormat = mPendingFormat.remove();
                }
                return mPendingOutputBuffers.remove();
            }
        }

        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int status = MediaCodecStatus.ERROR;
        int index = -1;
        try {
            int indexOrStatus = dequeueOutputBufferInternal(info, timeoutUs);
            updateLastPresentationTime(info);

            if (indexOrStatus >= 0) { // index!
                status = MediaCodecStatus.OK;
                index = indexOrStatus;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                mOutputBuffers = mMediaCodec.getOutputBuffers();
                status = MediaCodecStatus.OUTPUT_BUFFERS_CHANGED;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                status = MediaCodecStatus.OUTPUT_FORMAT_CHANGED;
                MediaFormat newFormat = mMediaCodec.getOutputFormat();
            } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                status = MediaCodecStatus.TRY_AGAIN_LATER;
            } else {
                Log.e(TAG, "Unexpected index_or_status: " + indexOrStatus);
                assert false;
            }
        } catch (IllegalStateException e) {
            status = MediaCodecStatus.ERROR;
            Log.e(TAG, "Failed to dequeue output buffer", e);
        }

        return new DequeueOutputResult(
                status, index, info.flags, info.offset, info.presentationTimeUs, info.size);
    }

    protected int dequeueOutputBufferInternal(MediaCodec.BufferInfo info, long timeoutUs) {
        return mMediaCodec.dequeueOutputBuffer(info, timeoutUs);
    }

    // TODO(sanfin): Move this out of MediaCodecBridge.
    boolean configureVideo(MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
        try {
            mMediaCodec.configure(format, surface, crypto, flags);
            return true;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot configure the video codec, wrong format or surface", e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot configure the video codec", e);
        } catch (MediaCodec.CryptoException e) {
            Log.e(TAG, "Cannot configure the video codec: DRM error", e);
        } catch (Exception e) {
            Log.e(TAG, "Cannot configure the video codec", e);
        }
        return false;
    }

    @TargetApi(Build.VERSION_CODES.M)
    @CalledByNative
    private boolean setSurface(Surface surface) {
        try {
            mMediaCodec.setOutputSurface(surface);
        } catch (IllegalArgumentException | IllegalStateException e) {
            Log.e(TAG, "Cannot set output surface", e);
            return false;
        }
        return true;
    }

    // TODO(sanfin): Move this out of MediaCodecBridge.
    boolean configureAudio(MediaFormat format, MediaCrypto crypto, int flags) {
        try {
            mMediaCodec.configure(format, null, crypto, flags);
            return true;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        } catch (MediaCodec.CryptoException e) {
            Log.e(TAG, "Cannot configure the audio codec: DRM error", e);
        } catch (Exception e) {
            Log.e(TAG, "Cannot configure the audio codec", e);
        }
        return false;
    }

    private void resetLastPresentationTimeIfNeeded(long presentationTimeUs) {
        if (mFlushed) {
            mLastPresentationTimeUs =
                    Math.max(presentationTimeUs - MAX_PRESENTATION_TIMESTAMP_SHIFT_US, 0);
            mFlushed = false;
        }
    }

    @SuppressWarnings("deprecation")
    private int getAudioFormat(int channelCount) {
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
    private static void createCallbackHandlerForTesting() {
        if (sCallbackHandlerThread != null) return;

        sCallbackHandlerThread = new HandlerThread("TestCallbackThread");
        sCallbackHandlerThread.start();
        sCallbackHandler = new Handler(sCallbackHandlerThread.getLooper());
    }

    private native void nativeOnBuffersAvailable(long nativeMediaCodecBridge);
}
