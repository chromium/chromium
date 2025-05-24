// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.media.MediaFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.Queue;

/** A MediaCodec wrapper for adapting the API and catching exceptions. */
@JNINamespace("media")
@NullMarked
class MediaCodecBridge {
    private static final String TAG = "MediaCodecBridge";

    private static final int MEDIA_CODEC_UNKNOWN_CIPHER_MODE = -1;

    // TODO(qinmin): Use MediaFormat constants when part of the public API.
    private static final String KEY_CROP_LEFT = "crop-left";
    private static final String KEY_CROP_RIGHT = "crop-right";
    private static final String KEY_CROP_BOTTOM = "crop-bottom";
    private static final String KEY_CROP_TOP = "crop-top";

    protected MediaCodec mMediaCodec;
    private final @BitrateAdjuster.Type int mBitrateAdjuster;

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
    private final boolean mUseAsyncApi;
    private Queue<MediaFormatWrapper> mPendingFormat;
    private @Nullable MediaFormatWrapper mCurrentFormat;
    private boolean mPendingError;
    private int mPendingErrorCode; // only valid if mPendingError = true
    private boolean mPendingStart;
    private long mNativeMediaCodecBridge;
    private int mSequenceCounter;
    private Queue<DequeueInputResult> mPendingInputBuffers;
    private Queue<DequeueOutputResult> mPendingOutputBuffers;

    // Cache for codec name array that is passed to LinearBlock.obtain().
    private String @Nullable [] mObtainBlockNames;

    // Set by tests which don't have a Java MessagePump to ensure the MediaCodec
    // callbacks are actually delivered. Always null in production.
    private static @Nullable HandlerThread sCallbackHandlerThread;
    private static @Nullable Handler sCallbackHandler;

    // |errorCode| is the error reported by MediaCodec.CryptoException.
    // As of API 31 (Android S) it returns MediaDrm.ErrorCodes
    // (https://developer.android.com/reference/android/media/MediaDrm.ErrorCodes).
    // Pre Android S exceptions are still compatible with this ordering, as MediaDrm.ErrorCodes and
    // MediaCodec.CryptoException.ErrorCodes are kept in sync.
    // (https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/media/java/android/media/MediaDrm.java;l=304-306)
    // Not all possible values are handled here, only the ones specified as being returned by
    // getErrorCode
    // (https://developer.android.com/reference/android/media/MediaCodec.CryptoException#getErrorCode())
    // Translated values are defined in media/base/android/media_codec_bridge.h.
    private static int translateCryptoException(int errorCode) {
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
            case MediaDrm.ErrorCodes.ERROR_CERTIFICATE_MALFORMED:
                return MediaCodecStatus.CERTIFICATE_MALFORMED;
            case MediaDrm.ErrorCodes.ERROR_CERTIFICATE_MISSING:
                return MediaCodecStatus.CERTIFICATE_MISSING;
            case MediaDrm.ErrorCodes.ERROR_CRYPTO_LIBRARY:
                return MediaCodecStatus.CRYPTO_LIBRARY;
            case MediaDrm.ErrorCodes.ERROR_GENERIC_OEM:
                return MediaCodecStatus.GENERIC_OEM;
            case MediaDrm.ErrorCodes.ERROR_GENERIC_PLUGIN:
                return MediaCodecStatus.GENERIC_PLUGIN;
            case MediaDrm.ErrorCodes.ERROR_INIT_DATA:
                return MediaCodecStatus.INIT_DATA;
            case MediaDrm.ErrorCodes.ERROR_KEY_NOT_LOADED:
                return MediaCodecStatus.KEY_NOT_LOADED;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_PARSE:
                return MediaCodecStatus.LICENSE_PARSE;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_POLICY:
                return MediaCodecStatus.LICENSE_POLICY;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_RELEASE:
                return MediaCodecStatus.LICENSE_RELEASE;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_REQUEST_REJECTED:
                return MediaCodecStatus.LICENSE_REQUEST_REJECTED;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_RESTORE:
                return MediaCodecStatus.LICENSE_RESTORE;
            case MediaDrm.ErrorCodes.ERROR_LICENSE_STATE:
                return MediaCodecStatus.LICENSE_STATE;
            case MediaDrm.ErrorCodes.ERROR_MEDIA_FRAMEWORK:
                return MediaCodecStatus.MEDIA_FRAMEWORK;
            case MediaDrm.ErrorCodes.ERROR_PROVISIONING_CERTIFICATE:
                return MediaCodecStatus.PROVISIONING_CERTIFICATE;
            case MediaDrm.ErrorCodes.ERROR_PROVISIONING_CONFIG:
                return MediaCodecStatus.PROVISIONING_CONFIG;
            case MediaDrm.ErrorCodes.ERROR_PROVISIONING_PARSE:
                return MediaCodecStatus.PROVISIONING_PARSE;
            case MediaDrm.ErrorCodes.ERROR_PROVISIONING_REQUEST_REJECTED:
                return MediaCodecStatus.PROVISIONING_REQUEST_REJECTED;
            case MediaDrm.ErrorCodes.ERROR_PROVISIONING_RETRY:
                return MediaCodecStatus.PROVISIONING_RETRY;
            case MediaDrm.ErrorCodes.ERROR_SECURE_STOP_RELEASE:
                return MediaCodecStatus.SECURE_STOP_RELEASE;
            case MediaDrm.ErrorCodes.ERROR_STORAGE_READ:
                return MediaCodecStatus.STORAGE_READ;
            case MediaDrm.ErrorCodes.ERROR_STORAGE_WRITE:
                return MediaCodecStatus.STORAGE_WRITE;
            case MediaDrm.ErrorCodes.ERROR_ZERO_SUBSAMPLES:
                return MediaCodecStatus.ZERO_SUBSAMPLES;
            default:
                Log.e(TAG, "Unknown MediaDrm.ErrorCodes error: " + errorCode);
                return MediaCodecStatus.UNKNOWN_MEDIADRM_EXCEPTION;
        }
    }

    private static int convertCryptoException(MediaCodec.CryptoException e) {
        return translateCryptoException(e.getErrorCode());
    }

    private static int convertCodecException(MediaCodec.CodecException e) {
        // https://developer.android.com/reference/android/media/MediaCodec.CodecException
        switch (e.getErrorCode()) {
            case MediaCodec.CodecException.ERROR_INSUFFICIENT_RESOURCE:
                return MediaCodecStatus.INSUFFICIENT_RESOURCE;
            case MediaCodec.CodecException.ERROR_RECLAIMED:
                return MediaCodecStatus.RECLAIMED;
            default:
                Log.e(TAG, "Unknown CodecException error: " + e.getErrorCode());
                if (e.getErrorCode() < 0) {
                    RecordHistogram.recordSparseHistogram(
                            "Media.MediaCodecError.NegativeCodecExceptionErrorCode",
                            e.getErrorCode());
                }
                return MediaCodecStatus.UNKNOWN_CODEC_EXCEPTION;
        }
    }

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
        private MediaCodec.@Nullable LinearBlock mBlock;
        private @Nullable ByteBuffer mBuffer;

        private ObtainBlockResult(
                MediaCodec.@Nullable LinearBlock block, @Nullable ByteBuffer buffer) {
            mBlock = block;
            mBuffer = buffer;
            assert (mBlock == null && mBuffer == null) || (mBlock != null && mBuffer != null);
        }

        @CalledByNative("ObtainBlockResult")
        private MediaCodec.@Nullable LinearBlock block() {
            return mBlock;
        }

        @CalledByNative("ObtainBlockResult")
        private @Nullable ByteBuffer buffer() {
            return mBuffer;
        }

        @CalledByNative("ObtainBlockResult")
        @SuppressLint("NewApi")
        private void recycle() {
            if (mBlock != null) {
                try {
                    mBlock.recycle();
                } catch (IllegalStateException ise) {
                    Log.e(TAG, "Failed to recyle LinearBlock: ", ise);
                }
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
        private final WeakReference<MediaCodecBridge> mMediaCodecBridgeRef;

        MediaCodecCallback(MediaCodecBridge bridge) {
            mMediaCodecBridgeRef = new WeakReference<>(bridge);
        }

        @Override
        public void onCryptoError(MediaCodec codec, MediaCodec.CryptoException e) {
            MediaCodecBridge bridge = mMediaCodecBridgeRef.get();
            if (bridge == null) {
                Log.d(TAG, "MediaCodecBridge was garbage collected, ignoring onCryptoError");
                return;
            }

            Log.e(TAG, "MediaCodec.onCryptoError: %s", e.getMessage());
            bridge.onError(convertCryptoException(e));
        }

        @Override
        public void onError(MediaCodec codec, MediaCodec.CodecException e) {
            // TODO(dalecurtis): We may want to drop transient errors here.
            MediaCodecBridge bridge = mMediaCodecBridgeRef.get();
            if (bridge == null) {
                Log.d(TAG, "MediaCodecBridge was garbage collected, ignoring onError");
                return;
            }

            Log.e(TAG, "MediaCodec.onError: %s", e.getDiagnosticInfo());
            bridge.onError(convertCodecException(e));
        }

        @Override
        public void onInputBufferAvailable(MediaCodec codec, int index) {
            MediaCodecBridge bridge = mMediaCodecBridgeRef.get();
            if (bridge == null) {
                Log.d(
                        TAG,
                        "MediaCodecBridge was garbage collected, ignoring onInputBufferAvailable");
                return;
            }

            bridge.onInputBufferAvailable(index);
        }

        @Override
        public void onOutputBufferAvailable(
                MediaCodec codec, int index, MediaCodec.BufferInfo info) {
            MediaCodecBridge bridge = mMediaCodecBridgeRef.get();
            if (bridge == null) {
                Log.d(
                        TAG,
                        "MediaCodecBridge was garbage collected, ignoring onOutputBufferAvailable");
                return;
            }

            bridge.onOutputBufferAvailable(index, info);
        }

        @Override
        public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            MediaCodecBridge bridge = mMediaCodecBridgeRef.get();
            if (bridge == null) {
                Log.d(
                        TAG,
                        "MediaCodecBridge was garbage collected, ignoring onOutputFormatChanged");
                return;
            }

            bridge.onOutputFormatChanged(format);
        }
    }
    ;

    MediaCodecBridge(
            MediaCodec mediaCodec, @BitrateAdjuster.Type int bitrateAdjuster, boolean useAsyncApi) {
        assert mediaCodec != null;
        mMediaCodec = mediaCodec;
        mBitrateAdjuster = bitrateAdjuster;

        try {
            mMediaCodecName = mediaCodec.getName();
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
        mPendingFormat = new ArrayDeque<MediaFormatWrapper>();
        mPendingInputBuffers = new ArrayDeque<DequeueInputResult>();
        mPendingOutputBuffers = new ArrayDeque<DequeueOutputResult>();
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

    public synchronized void onError(int errorCode) {
        mPendingError = true;
        mPendingErrorCode = errorCode;
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
    }

    // TODO(sanfin): Move this to constructor or builder.
    @SuppressWarnings("deprecation")
    boolean start() {
        try {
            if (mUseAsyncApi) {
                synchronized (this) {
                    if (mPendingError) return false;

                    class CompletePendingStartTask implements Runnable {
                        private final int mThisSequence;

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
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            mPendingErrorCode = convertCodecException(e);
            return false;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            mPendingErrorCode = MediaCodecStatus.ILLEGAL_STATE;
            return false;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot start the media codec", e);
            mPendingErrorCode = MediaCodecStatus.ERROR;
            return false;
        }
        return true;
    }

    @CalledByNative
    private DequeueInputResult dequeueInputBuffer(long timeoutUs) {
        if (mUseAsyncApi) {
            synchronized (this) {
                if (mPendingError) {
                    return new DequeueInputResult(mPendingErrorCode, -1);
                }
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
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Failed to dequeue input buffer", e);
            status = convertCodecException(e);
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
            if (mObtainBlockNames == null) {
                mObtainBlockNames = new String[1];
                mObtainBlockNames[0] = mMediaCodecName;
            }
            block = MediaCodec.LinearBlock.obtain(capacity < 16 ? 16 : capacity, mObtainBlockNames);
            if (block != null) {
                buffer = block.map();
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to obtain LinearBlock", e);
            if (block != null) {
                assert buffer == null;
                try {
                    block.recycle();
                } catch (IllegalStateException ise) {
                    Log.e(TAG, "Failed to recyle LinearBlock after map failure: ", ise);
                }
                block = null;
            }
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
                if (!start()) {
                    return mPendingErrorCode;
                }
            }
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Failed to flush MediaCodec", e);
            return convertCodecException(e);
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
    private @Nullable MediaFormatWrapper getOutputFormat() {
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
    private @Nullable MediaFormatWrapper getInputFormat() {
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
    private @Nullable ByteBuffer getInputBuffer(int index) {
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
    protected @Nullable ByteBuffer getOutputBuffer(int index) {
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
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Failed to queue input buffer", e);
            return convertCodecException(e);
        } catch (MediaCodec.CryptoException e) {
            Log.e(TAG, "Failed to queue input buffer", e);
            return convertCryptoException(e);
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
            return MediaCodecStatus.INPUT_SLOT_UNAVAILABLE;
        }
        return MediaCodecStatus.OK;
    }

    private static final String QUEUE_SECURE_INPUT_BLOCK_ERR_MSG =
            "Failed to queue secure input block: ";

    @CalledByNative
    @SuppressLint("NewApi")
    private int queueSecureInputBlock(
            int index,
            MediaCodec.LinearBlock block,
            int offset,
            int size,
            byte[] iv,
            byte[] keyId,
            int[] numBytesOfClearData,
            int[] numBytesOfEncryptedData,
            int numSubSamples,
            int cipherMode,
            int patternEncrypt,
            int patternSkip,
            long presentationTimeUs,
            int flags) {
        try {
            cipherMode = translateEncryptionSchemeValue(cipherMode);

            var status = validateCryptoInfo(cipherMode, patternEncrypt, patternSkip);
            if (status != MediaCodecStatus.OK) {
                return status;
            }

            var cryptoInfo =
                    generateCryptoInfo(
                            iv,
                            keyId,
                            numBytesOfClearData,
                            numBytesOfEncryptedData,
                            numSubSamples,
                            cipherMode,
                            patternEncrypt,
                            patternSkip);
            assert cryptoInfo != null;

            MediaCodec.QueueRequest request = mMediaCodec.getQueueRequest(index);
            request.setEncryptedLinearBlock(block, offset, size, cryptoInfo);
            request.setPresentationTimeUs(presentationTimeUs);
            request.setFlags(flags);
            request.queue();
        } catch (MediaCodec.CryptoException e) {
            if (e.getErrorCode() == MediaDrm.ErrorCodes.ERROR_NO_KEY) {
                Log.d(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG + "CryptoException.ERROR_NO_KEY");
                return MediaCodecStatus.NO_KEY;
            }
            // Anything other than ERROR_NO_KEY is unexpected.
            Log.e(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG, e);
            return convertCryptoException(e);
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG, e.getDiagnosticInfo());
            return convertCodecException(e);
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException can occur when release() is called on the MediaCrypto
            // object, but the MediaCodecBridge is unaware of the change.
            Log.e(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG, e);
            return MediaCodecStatus.ERROR;
        } catch (IllegalStateException e) {
            Log.e(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG, e);
            return MediaCodecStatus.ILLEGAL_STATE;
        } catch (Exception e) {
            Log.e(TAG, QUEUE_SECURE_INPUT_BLOCK_ERR_MSG, e);
            return MediaCodecStatus.INPUT_SLOT_UNAVAILABLE;
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

    private int validateCryptoInfo(int cipherMode, int patternEncrypt, int patternSkip) {
        if (cipherMode == MEDIA_CODEC_UNKNOWN_CIPHER_MODE) {
            return MediaCodecStatus.UNKNOWN_CIPHER_MODE;
        }
        if (cipherMode != MediaCodec.CRYPTO_MODE_AES_CBC
                && patternEncrypt != 0
                && patternSkip != 0) {
            Log.e(TAG, "Pattern encryption only supported for 'cbcs' scheme (CBC mode).");
            return MediaCodecStatus.PATTERN_ENCRYPTION_NOT_SUPPORTED;
        }
        return MediaCodecStatus.OK;
    }

    private CryptoInfo generateCryptoInfo(
            byte[] iv,
            byte[] keyId,
            int[] numBytesOfClearData,
            int[] numBytesOfEncryptedData,
            int numSubSamples,
            int cipherMode,
            int patternEncrypt,
            int patternSkip) {
        var cryptoInfo = new CryptoInfo();
        cryptoInfo.set(
                numSubSamples, numBytesOfClearData, numBytesOfEncryptedData, keyId, iv, cipherMode);
        if (cipherMode == MediaCodec.CRYPTO_MODE_AES_CBC
                && patternEncrypt != 0
                && patternSkip != 0) {
            cryptoInfo.setPattern(new CryptoInfo.Pattern(patternEncrypt, patternSkip));
        }
        return cryptoInfo;
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

            var status = validateCryptoInfo(cipherMode, patternEncrypt, patternSkip);
            if (status != MediaCodecStatus.OK) {
                return status;
            }

            var cryptoInfo =
                    generateCryptoInfo(
                            iv,
                            keyId,
                            numBytesOfClearData,
                            numBytesOfEncryptedData,
                            numSubSamples,
                            cipherMode,
                            patternEncrypt,
                            patternSkip);
            assert cryptoInfo != null;
            mMediaCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
        } catch (MediaCodec.CryptoException e) {
            if (e.getErrorCode() == MediaDrm.ErrorCodes.ERROR_NO_KEY) {
                Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
                return MediaCodecStatus.NO_KEY;
            }
            // Anything other than ERROR_NO_KEY is unexpected.
            Log.e(
                    TAG,
                    "Failed to queue secure input buffer, CryptoException.ErrorCode: "
                            + e.getErrorCode());
            return convertCryptoException(e);
        } catch (MediaCodec.CodecException e) {
            Log.e(TAG, "Failed to queue secure input buffer.", e);
            Log.e(TAG, "Diagnostic: %s", e.getDiagnosticInfo());
            return convertCodecException(e);
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException can occur when release() is called on the MediaCrypto
            // object, but the MediaCodecBridge is unaware of the change.
            Log.e(TAG, "Failed to queue secure input buffer.", e);
            return MediaCodecStatus.ERROR;
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to queue secure input buffer.", e);
            return MediaCodecStatus.ILLEGAL_STATE;
        }
        return MediaCodecStatus.OK;
    }

    @CalledByNative
    protected void releaseOutputBuffer(int index, boolean render) {
        if (mUseAsyncApi) {
            synchronized (this) {
                if (mPendingError) {
                    Log.e(TAG, "Skipping releaseOutputBuffer() due to codec errors.");
                    return;
                }
            }
        }
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
                    return new DequeueOutputResult(mPendingErrorCode, -1, 0, 0, 0, 0);
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
            status = MediaCodecStatus.ILLEGAL_STATE;
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
    boolean configureVideo(
            MediaFormat format,
            @Nullable Surface surface,
            @Nullable MediaCrypto crypto,
            int flags) {
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
                if (inputFormat.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)) {
                    mMaxInputSize = inputFormat.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE);
                }
            }

            // Aligned resolutions are only required for encoding.
            if ((flags & MediaCodec.CONFIGURE_FLAG_ENCODE) == 0) return true;

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
