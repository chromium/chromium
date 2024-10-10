// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;

import java.nio.ByteBuffer;
import java.util.LinkedList;
import java.util.Queue;

/** A MediaCodec wrapper for adapting the API and catching exceptions. */
@JNINamespace("media")
class MediaCodecBridge {
    private static final String TAG = "MediaCodecBridge";

    private static final int MEDIA_CODEC_UNKNOWN_CIPHER_MODE = -1;

    // TODO(qinmin): Use MediaFormat constants when part of the public API.
    private static final String KEY_CROP_LEFT = "crop-left";
    private static final String KEY_CROP_RIGHT = "crop-right";
    private static final String KEY_CROP_BOTTOM = "crop-bottom";
    private static final String KEY_CROP_TOP = "crop-top";

    protected MediaCodec mMediaCodec;
    private @BitrateAdjuster.Type int mBitrateAdjuster;

    private String mMediaCodecName = "unknown";

    // The maximum input size this codec was configured with.
    private int mMaxInputSize;

    // To support both the synchronous and asynchronous version of MediaCodec
    // (since we need to work on <M devices), we implement async support as a
    // layer under synchronous API calls and provide a callback signal for when
    // work (new input, new output, errors, or format changes) is available.
    //
    // Once the callback has been set on MediaCodec, these variables must only
    // be accessed from synchronized(this) blocks since MediaCodecCallback may
    // execute on an arbitrary thread.
    private boolean mUseAsyncApi;
    private Queue<MediaFormatWrapper> mPendingFormat;
    private MediaFormatWrapper mCurrentFormat;
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

    private static class ObtainBlockResult {
        private MediaCodec.LinearBlock mBlock;
        private ByteBuffer mBuffer;

        private ObtainBlockResult(MediaCodec.LinearBlock block, ByteBuffer buffer) {
            mBlock = block;
            mBuffer = buffer;
        }

        @CalledByNative("ObtainBlockResult")
        private MediaCodec.LinearBlock block() {
            return mBlock;
        }

        @CalledByNative("ObtainBlockResult")
        private ByteBuffer buffer() {
            return mBuffer;
        }

        @CalledByNative("ObtainBlockResult")
        @SuppressLint("NewApi")
        private void recycle() {
            if (mBlock != null) {
                mBlock.recycle();
                mBlock = null;
                mBuffer = null;
            }
        }
    }

    private static class DequeueOutputResult {
        private final int mStatus;
        private final int mIndex;
        private final int mFlags;
        private final int mOffset;
        private final long mPresentationTimeMicroseconds;
        private final int mNumBytes;

        private DequeueOutputResult(
                int status,
                int index,
                int flags,
                int offset,
                long presentationTimeMicroseconds,
                int numBytes) {
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
    private static class MediaFormatWrapper {
        private final MediaFormat mFormat;

        private MediaFormatWrapper(MediaFormat format) {
            mFormat = format;
        }

        private boolean formatHasCropValues() {
            return mFormat.containsKey(KEY_CROP_RIGHT)
                    && mFormat.containsKey(KEY_CROP_LEFT)
                    && mFormat.containsKey(KEY_CROP_BOTTOM)
                    && mFormat.containsKey(KEY_CROP_TOP);
        }

        @CalledByNative("MediaFormatWrapper")
        private int width() {
            return formatHasCropValues()
                    ? mFormat.getInteger(KEY_CROP_RIGHT) - mFormat.getInteger(KEY_CROP_LEFT) + 1
                    : mFormat.getInteger(MediaFormat.KEY_WIDTH);
        }

        @CalledByNative("MediaFormatWrapper")
        private int height() {
            return formatHasCropValues()
                    ? mFormat.getInteger(KEY_CROP_BOTTOM) - mFormat.getInteger(KEY_CROP_TOP) + 1
                    : mFormat.getInteger(MediaFormat.KEY_HEIGHT);
        }

        @CalledByNative("MediaFormatWrapper")
        private int sampleRate() {
            return mFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
        }

        @CalledByNative("MediaFormatWrapper")
        private int channelCount() {
            return mFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
        }

        @CalledByNative("MediaFormatWrapper")
        private int stride() {
            // Missing stride means a 16x16 resolution alignment is required. See configureVideo().
            if (!mFormat.containsKey(MediaFormat.KEY_STRIDE)) return width();
            return mFormat.getInteger(MediaFormat.KEY_STRIDE);
        }

        @CalledByNative("MediaFormatWrapper")
        private int yPlaneHeight() {
            // Missing stride means a 16x16 resolution alignment is required. See configureVideo().
            if (!mFormat.containsKey(MediaFormat.KEY_SLICE_HEIGHT)) return height();
            return mFormat.getInteger(MediaFormat.KEY_SLICE_HEIGHT);
        }

        @CalledByNative("MediaFormatWrapper")
        private int colorStandard() {
            if (!mFormat.containsKey(MediaFormat.KEY_COLOR_STANDARD)) return -1;
            return mFormat.getInteger(MediaFormat.KEY_COLOR_STANDARD);
        }

        @CalledByNative("MediaFormatWrapper")
        private int colorRange() {
            if (!mFormat.containsKey(MediaFormat.KEY_COLOR_RANGE)) return -1;
            return mFormat.getInteger(MediaFormat.KEY_COLOR_RANGE);
        }

        @CalledByNative("MediaFormatWrapper")
        private int colorTransfer() {
            if (!mFormat.containsKey(MediaFormat.KEY_COLOR_TRANSFER)) return -1;
            return mFormat.getInteger(MediaFormat.KEY_COLOR_TRANSFER);
        }
    }

    // Warning: This class may execute on an arbitrary thread for the lifetime
    // of the MediaCodec. The MediaCodecBridge methods it calls are synchronized
    // to avoid race conditions.
    static class MediaCodecCallback extends MediaCodec.Callback {
        private MediaCodecBridge mMediaCodecBridge;

        MediaCodecCallback(MediaCodecBridge bridge) {
            mMediaCodecBridge = bridge;
        }

        @Override
        public void onError(MediaCodec codec, MediaCodec.CodecException e) {
            // TODO(dalecurtis): We may want to drop transient errors here.
            Log.e(TAG, "MediaCodec.onError: %s", e.getDiagnosticInfo());
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
    }
    ;

    MediaCodecBridge(
            MediaCodec mediaCodec, @BitrateAdjuster.Type int bitrateAdjuster, boolean useAsyncApi) {
        assert mediaCodec != null;
        mMediaCodec = mediaCodec;
        mBitrateAdjuster = bitrateAdjuster;

        try {
            mMediaCodecName = mMediaCodec.getName();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot get codec name", e);
        }

        mUseAsyncApi = useAsyncApi;
        if (!mUseAsyncApi) return;

        enableAsyncApi();
        prepareAsyncApiForRestart();
    }

    private void enableAsyncApi() {
        mPendingError = false;
        mPendingFormat = new LinkedList<MediaFormatWrapper>();
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
        if (!mPendingInputBuffers.isEmpty() || !mPendingOutputBuffers.isEmpty() || mPendingError) {
            notifyBuffersAvailable();
        }
    }

    private synchronized void notifyBuffersAvailable() {
        if (mNativeMediaCodecBridge != 0) {
            MediaCodecBridgeJni.get()
                    .onBuffersAvailable(mNativeMediaCodecBridge, MediaCodecBridge.this);
        }
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

        mPendingOutputBuffers.add(
                new DequeueOutputResult(
                        MediaCodecStatus.OK,
                        index,
                        info.flags,
                        info.offset,
                        info.presentationTimeUs,
                        info.size));
        notifyBuffersAvailable();
    }

    public synchronized void onOutputFormatChanged(MediaFormat format) {
        mPendingOutputBuffers.add(
                new DequeueOutputResult(MediaCodecStatus.OUTPUT_FORMAT_CHANGED, -1, 0, 0, 0, 0));
        mPendingFormat.add(new MediaFormatWrapper(format));
        notifyBuffersAvailable();
    }

    public synchronized void onPendingStartComplete(int sequenceCounter) {
        // Ignore events from the past.
        if (mSequenceCounter != sequenceCounter) return;
        mPendingStart = false;
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
            // This logging is to help us identify hung MediaCodecs in crash reports.
            Log.w(TAG, "Releasing: %s", mMediaCodecName);
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
                    }

                    // Ensure any pending indices are ignored until after start
                    // by trampolining through the handler/looper that the
                    // notifications are coming from.
                    Handler h =
                            sCallbackHandler == null
                                    ? new Handler(Looper.getMainLooper())
                                    : sCallbackHandler;
                    h.post(new CompletePendingStartTask(mSequenceCounter));
                }
            }

            mMediaCodec.start();
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
                if (mPendingStart || mPendingInputBuffers.isEmpty()) {
                    return new DequeueInputResult(MediaCodecStatus.TRY_AGAIN_LATER, -1);
                }
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
                Log.e(TAG, "Unexpected index_or_status: %d", indexOrStatus);
                assert false;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to dequeue input buffer", e);
        }
        return new DequeueInputResult(status, index);
    }

    @CalledByNative
    @SuppressLint("NewApi")
    private ObtainBlockResult obtainBlock(int capacity) {
        MediaCodec.LinearBlock block = null;
        ByteBuffer buffer = null;
        try {
            // TODO(crbug.com/327625558): Store as member to avoid frequent allocations.
            String[] names = new String[1];
            names[0] = mMediaCodecName;
            block = MediaCodec.LinearBlock.obtain(capacity < 16 ? 16 : capacity, names);
            if (block != null) {
                buffer = block.map();
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to obtain LinearBlock", e);
        }
        return new ObtainBlockResult(block, buffer);
    }

    @CalledByNative
    private int flush() {
        try {
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

    @CalledByNative
    private String getName() {
        return mMediaCodecName;
    }

    @CalledByNative
    private boolean isSoftwareCodec() {
        return MediaCodecUtil.isSoftwareCodec(mMediaCodec.getCodecInfo());
    }

    @CalledByNative
    private MediaFormatWrapper getOutputFormat() {
        if (mUseAsyncApi && mCurrentFormat != null) return mCurrentFormat;

        try {
            MediaFormat format = mMediaCodec.getOutputFormat();
            if (format != null) return new MediaFormatWrapper(format);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to get output format", e);
        }
        return null;
    }

    @CalledByNative
    private MediaFormatWrapper getInputFormat() {
        try {
            MediaFormat format = mMediaCodec.getInputFormat();
            if (format != null) return new MediaFormatWrapper(format);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to get input format", e);
        }
        return null;
    }

    /** Returns null if MediaCodec throws IllegalStateException. */
    @CalledByNative
    private ByteBuffer getInputBuffer(int index) {
        if (mUseAsyncApi) {
            synchronized (this) {
                if (mPendingError) return null;
            }
        }
        try {
            return mMediaCodec.getInputBuffer(index);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to get input buffer", e);
            return null;
        }
    }

    /** Returns null if MediaCodec throws IllegalStateException. */
    @CalledByNative
    protected ByteBuffer getOutputBuffer(int index) {
        try {
            return mMediaCodec.getOutputBuffer(index);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to get output buffer", e);
            return null;
        }
    }

    @CalledByNative
    private int queueInputBuffer(
            int index, int offset, int size, long presentationTimeUs, int flags) {
        try {
            mMediaCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
        } catch (Exception e) {
            Log.e(TAG, "Failed to queue input buffer", e);
            return MediaCodecStatus.ERROR;
        }
        return MediaCodecStatus.OK;
    }

    @CalledByNative
    @SuppressLint("NewApi")
    private int queueInputBlock(
            int index,
            MediaCodec.LinearBlock block,
            int offset,
            int size,
            long presentationTimeUs,
            int flags) {
        try {
            MediaCodec.QueueRequest request = mMediaCodec.getQueueRequest(index);
            request.setLinearBlock(block, offset, size);
            request.setPresentationTimeUs(presentationTimeUs);
            request.setFlags(flags);
            request.queue();
        } catch (Exception e) {
            Log.e(TAG, "Failed to queue input block", e);
            return MediaCodecStatus.ERROR;
        }
        return MediaCodecStatus.OK;
    }

    @CalledByNative
    private void setVideoBitrate(int bps, int frameRate) {
        int targetBps = BitrateAdjuster.getTargetBitrate(mBitrateAdjuster, bps, frameRate);
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, targetBps);
        try {
            mMediaCodec.setParameters(b);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to set MediaCodec parameters", e);
        }
        Log.v(TAG, "setVideoBitrate: input %dbps@%d, targetBps %d", bps, frameRate, targetBps);
    }

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
    private int translateEncryptionSchemeValue(int nativeValue) {
        switch (nativeValue) {
            case EncryptionScheme.UNENCRYPTED:
                return MediaCodec.CRYPTO_MODE_UNENCRYPTED;
            case EncryptionScheme.CENC:
                return MediaCodec.CRYPTO_MODE_AES_CTR;
            case EncryptionScheme.CBCS:
                return MediaCodec.CRYPTO_MODE_AES_CBC;
            default:
                Log.e(TAG, "Unsupported cipher mode: %d", nativeValue);
                return MEDIA_CODEC_UNKNOWN_CIPHER_MODE;
        }
    }

    // |errorCode| is the error reported by MediaCodec.CryptoException
    // (https://developer.android.com/reference/android/media/MediaCodec.CryptoException)
    // Translated values are defined in media/base/android/media_codec_bridge.h.
    // MediaCodec.CryptoException error codes were deprecated in API 31 (Android S) and replaced by
    // MediaDrm error codes.
    private int translateCryptoExceptionPreS(int errorCode) {
        switch (errorCode) {
            case MediaCodec.CryptoException.ERROR_NO_KEY:
                return MediaCodecStatus.NO_KEY;
            case MediaCodec.CryptoException.ERROR_KEY_EXPIRED:
                return MediaCodecStatus.KEY_EXPIRED;
            case MediaCodec.CryptoException.ERROR_RESOURCE_BUSY:
                return MediaCodecStatus.RESOURCE_BUSY;
            case MediaCodec.CryptoException.ERROR_INSUFFICIENT_OUTPUT_PROTECTION:
                return MediaCodecStatus.INSUFFICIENT_OUTPUT_PROTECTION;
            case MediaCodec.CryptoException.ERROR_SESSION_NOT_OPENED:
                return MediaCodecStatus.SESSION_NOT_OPENED;
            case MediaCodec.CryptoException.ERROR_UNSUPPORTED_OPERATION:
                return MediaCodecStatus.UNSUPPORTED_OPERATION;
            case 7: // ERROR_INSUFFICIENT_SECURITY, added in API 29
                return MediaCodecStatus.INSUFFICIENT_SECURITY;
            case 8: // ERROR_FRAME_TOO_LARGE, added in API 29
                return MediaCodecStatus.FRAME_TOO_LARGE;
            case 9: // ERROR_LOST_STATE, added in API 29
                return MediaCodecStatus.LOST_STATE;
            default:
                Log.e(TAG, "Unknown CryptoException error code: " + errorCode);
                return MediaCodecStatus.ERROR;
        }
    }

    // |errorCode| is the error reported by MediaCodec.CryptoException.
    // As of API 31 (Android S) it returns MediaDrm.ErrorCodes
    // (https://developer.android.com/reference/android/media/MediaDrm.ErrorCodes).
    // Not all possible values are handled here, only the ones specified as being returned by
    // getErrorCode
    // (https://developer.android.com/reference/android/media/MediaCodec.CryptoException#getErrorCode())
    // Translated values are defined in media/base/android/media_codec_bridge.h.
    @RequiresApi(Build.VERSION_CODES.S)
    private int translateCryptoExceptionPostS(int errorCode) {
        switch (errorCode) {
            case MediaDrm.ErrorCodes.ERROR_NO_KEY:
                return MediaCodecStatus.NO_KEY;
            case MediaDrm.ErrorCodes.ERROR_KEY_EXPIRED:
                return MediaCodecStatus.KEY_EXPIRED;
            case MediaDrm.ErrorCodes.ERROR_RESOURCE_BUSY:
                return MediaCodecStatus.RESOURCE_BUSY;
            case MediaDrm.ErrorCodes.ERROR_INSUFFICIENT_OUTPUT_PROTECTION:
                return MediaCodecStatus.INSUFFICIENT_OUTPUT_PROTECTION;
            case MediaDrm.ErrorCodes.ERROR_SESSION_NOT_OPENED:
                return MediaCodecStatus.SESSION_NOT_OPENED;
            case MediaDrm.ErrorCodes.ERROR_UNSUPPORTED_OPERATION:
                return MediaCodecStatus.UNSUPPORTED_OPERATION;
            case MediaDrm.ErrorCodes.ERROR_INSUFFICIENT_SECURITY:
                return MediaCodecStatus.INSUFFICIENT_SECURITY;
            case MediaDrm.ErrorCodes.ERROR_FRAME_TOO_LARGE:
                return MediaCodecStatus.FRAME_TOO_LARGE;
            case MediaDrm.ErrorCodes.ERROR_LOST_STATE:
                return MediaCodecStatus.LOST_STATE;
            case MediaDrm.ErrorCodes.ERROR_GENERIC_OEM:
                return MediaCodecStatus.GENERIC_OEM;
            case MediaDrm.ErrorCodes.ERROR_GENERIC_PLUGIN:
                return MediaCodecStatus.GENERIC_PLUGIN;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_PARSE:
                return MediaCodecStatus.LICENSE_PARSE;
            case MediaDrm.ErrorCodes.ERROR_MEDIA_FRAMEWORK:
                return MediaCodecStatus.MEDIA_FRAMEWORK;
            case MediaDrm.ErrorCodes.ERROR_ZERO_SUBSAMPLES:
                return MediaCodecStatus.ZERO_SUBSAMPLES;
            default:
                Log.e(TAG, "Unknown MediaDrm.ErrorCodes error: " + errorCode);
                return MediaCodecStatus.ERROR;
        }
    }

    @SuppressLint("WrongConstant") // False positive on logging statement.
    @CalledByNative
    private int queueSecureInputBuffer(
            int index,
            int offset,
            byte[] iv,
            byte[] keyId,
            int[] numBytesOfClearData,
            int[] numBytesOfEncryptedData,
            int numSubSamples,
            int cipherMode,
            int patternEncrypt,
            int patternSkip,
            long presentationTimeUs) {
        try {
            cipherMode = translateEncryptionSchemeValue(cipherMode);
            if (cipherMode == MEDIA_CODEC_UNKNOWN_CIPHER_MODE) {
                return MediaCodecStatus.UNKNOWN_CIPHER_MODE;
            }
            boolean usesCbcs = cipherMode == MediaCodec.CRYPTO_MODE_AES_CBC;
            CryptoInfo cryptoInfo = new CryptoInfo();
            cryptoInfo.set(
                    numSubSamples,
                    numBytesOfClearData,
                    numBytesOfEncryptedData,
                    keyId,
                    iv,
                    cipherMode);
            if (patternEncrypt != 0 && patternSkip != 0) {
                if (usesCbcs) {
                    // Above platform check ensured that setting the pattern is indeed supported.
                    MediaCodecUtil.setPatternIfSupported(cryptoInfo, patternEncrypt, patternSkip);
                } else {
                    Log.e(TAG, "Pattern encryption only supported for 'cbcs' scheme (CBC mode).");
                    return MediaCodecStatus.PATTERN_ENCRYPTION_NOT_SUPPORTED;
                }
            }
            mMediaCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
        } catch (MediaCodec.CryptoException e) {
            if (e.getErrorCode() == MediaCodec.CryptoException.ERROR_NO_KEY) {
                Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
                return MediaCodecStatus.NO_KEY;
            }
            // Anything other than ERROR_NO_KEY is unexpected.
            Log.e(
                    TAG,
                    "Failed to queue secure input buffer, CryptoException.ErrorCode: "
                            + e.getErrorCode());
            return (Build.VERSION.SDK_INT < Build.VERSION_CODES.S)
                    ? translateCryptoExceptionPreS(e.getErrorCode())
                    : translateCryptoExceptionPostS(e.getErrorCode());
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Failed to queue secure input buffer.", e);
            Log.e(TAG, "Diagnostic: %s", e.getDiagnosticInfo());
            return MediaCodecStatus.ERROR;
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException can occur when release() is called on the MediaCrypto
            // object, but the MediaCodecBridge is unaware of the change.
            Log.e(TAG, "Failed to queue secure input buffer.", e);
            return MediaCodecStatus.ERROR;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to queue secure input buffer.", e);
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
                if (mPendingError) {
                    return new DequeueOutputResult(MediaCodecStatus.ERROR, -1, 0, 0, 0, 0);
                }
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

            if (indexOrStatus >= 0) { // index!
                status = MediaCodecStatus.OK;
                index = indexOrStatus;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                status = MediaCodecStatus.OUTPUT_BUFFERS_CHANGED;
            } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                status = MediaCodecStatus.OUTPUT_FORMAT_CHANGED;
            } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                status = MediaCodecStatus.TRY_AGAIN_LATER;
            } else {
                Log.e(TAG, "Unexpected index_or_status: %d", indexOrStatus);
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

    private static int alignDown(int size, int alignment) {
        return size & ~(alignment - 1);
    }

    @SuppressLint("NewApi")
    boolean configureVideo(MediaFormat format, Surface surface, MediaCrypto crypto, int flags) {
        try {
            if ((flags & MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL) != 0) {
                format.removeKey(MediaFormat.KEY_MAX_INPUT_SIZE);
            }

            mMediaCodec.configure(format, surface, crypto, flags);

            MediaFormat inputFormat = mMediaCodec.getInputFormat();

            if ((flags & MediaCodec.CONFIGURE_FLAG_USE_BLOCK_MODEL) != 0) {
                mMaxInputSize = Integer.MAX_VALUE;
            } else {
                // This is always provided by MediaFormatBuilder, but we should see if the input
                // format has the real value.
                mMaxInputSize = format.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE);
                if (flags != MediaCodec.CONFIGURE_FLAG_ENCODE) {
                    if (inputFormat.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)) {
                        mMaxInputSize = inputFormat.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE);
                    }
                    return true;
                }
            }

            // Non 16x16 aligned resolutions don't work well with the MediaCodec encoder
            // unfortunately, see https://crbug.com/1084702 for details. It seems they
            // only work when the stride and slice height information are provided.
            boolean requireAlignedResolution =
                    !inputFormat.containsKey(MediaFormat.KEY_STRIDE)
                            || !inputFormat.containsKey(MediaFormat.KEY_SLICE_HEIGHT);

            if (!requireAlignedResolution) return true;

            int currentWidth = inputFormat.getInteger(MediaFormat.KEY_WIDTH);
            int alignedWidth = alignDown(currentWidth, 16);

            int currentHeight = inputFormat.getInteger(MediaFormat.KEY_HEIGHT);
            int alignedHeight = alignDown(currentHeight, 16);

            if (alignedHeight == 0 || alignedWidth == 0) {
                Log.e(
                        TAG,
                        "MediaCodec requires 16x16 alignment, which is not possible for: "
                                + currentWidth
                                + "x"
                                + currentHeight);
                return false;
            }

            if (alignedWidth == currentWidth && alignedHeight == currentHeight) return true;

            // We must reconfigure the MediaCodec now since setParameters() doesn't work
            // consistently across devices and versions of Android.
            mMediaCodec.reset();
            format.setInteger(MediaFormat.KEY_WIDTH, alignedWidth);
            format.setInteger(MediaFormat.KEY_HEIGHT, alignedHeight);
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
                return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
            default:
                return AudioFormat.CHANNEL_OUT_DEFAULT;
        }
    }

    @CalledByNative
    private int getMaxInputSize() {
        return mMaxInputSize;
    }

    @CalledByNative
    private static void createCallbackHandlerForTesting() {
        if (sCallbackHandlerThread != null) return;

        sCallbackHandlerThread = new HandlerThread("TestCallbackThread");
        sCallbackHandlerThread.start();
        sCallbackHandler = new Handler(sCallbackHandlerThread.getLooper());
    }

    @NativeMethods
    interface Natives {
        void onBuffersAvailable(long nativeMediaCodecBridge, MediaCodecBridge caller);
    }
}
