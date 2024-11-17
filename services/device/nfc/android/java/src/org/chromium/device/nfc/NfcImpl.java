// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.os.Process;
import android.os.Vibrator;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.device.mojom.NdefError;
import org.chromium.device.mojom.NdefErrorType;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefWriteOptions;
import org.chromium.device.mojom.Nfc;
import org.chromium.device.mojom.NfcClient;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.bindings.Router;
import org.chromium.mojo.system.MojoException;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

/**
 * Android implementation of the NFC mojo service defined in services/device/public/mojom/nfc.mojom.
 */
public class NfcImpl implements Nfc {
    private static final String TAG = "NfcImpl";
    private static final long MIN_TIME_BETWEEN_VIBRATIONS_MS = 1000;
    private static final long DELAY_TO_DISABLE_READER_MODE_MS = 500;

    private final int mHostId;

    private final NfcDelegate mDelegate;

    private Router mRouter;

    /** Used to get instance of NFC adapter, @see android.nfc.NfcManager */
    private final NfcManager mNfcManager;

    /** NFC adapter. @see android.nfc.NfcAdapter */
    private final NfcAdapter mNfcAdapter;

    /**
     * Activity that is in foreground and is used to enable / disable NFC reader mode operations.
     * Can be updated when activity associated with web page is changed. @see #setActivity
     */
    private Activity mActivity;

    /** Flag that indicates whether NFC permission is granted. */
    private final boolean mHasPermission;

    /**
     * Flag that indicates whether NFC operations are suspended. Is updated when
     * suspendNfcOperations() and resumeNfcOperations() are called.
     */
    private boolean mOperationsSuspended;

    /** Implementation of android.nfc.NfcAdapter.ReaderCallback. @see ReaderCallbackHandler */
    private ReaderCallbackHandler mReaderCallbackHandler;

    /**
     * Object that contains data that was passed to method
     * #push(NdefMessage message, NdefWriteOptions options, Push_Response callback)
     * @see PendingPushOperation
     */
    private PendingPushOperation mPendingPushOperation;

    /**
     * Object that contains the callback that was passed to method
     * #makeReadOnly(MakeReadOnly_Response callback)
     * @see PendingMakeReadOnlyOperation
     */
    private PendingMakeReadOnlyOperation mPendingMakeReadOnlyOperation;

    /**
     * Utility that provides I/O operations for a Tag. Created on demand when Tag is found.
     * @see NfcTagHandler
     */
    private NfcTagHandler mTagHandler;

    /**
     * Client interface used to deliver NdefMessages for registered watch operations.
     * @see #watch
     */
    private NfcClient mClient;

    private final List<Integer> mWatchIds = new ArrayList<Integer>();

    /** Vibrator. @see android.os.Vibrator */
    private Vibrator mVibrator;

    /** Last time in milliseconds when a Tag was discovered. */
    private long mTagDiscoveredLastTimeMs = -1;

    public NfcImpl(int hostId, NfcDelegate delegate, InterfaceRequest<Nfc> request) {
        mHostId = hostId;
        mDelegate = delegate;
        mOperationsSuspended = false;

        // |request| may be null in tests.
        if (request != null) {
            mRouter = Nfc.MANAGER.bind(this, request);
        }

        int permission =
                ContextUtils.getApplicationContext()
                        .checkPermission(Manifest.permission.NFC, Process.myPid(), Process.myUid());
        mHasPermission = permission == PackageManager.PERMISSION_GRANTED;
        Callback<Activity> onActivityUpdatedCallback =
                new Callback<Activity>() {
                    @Override
                    public void onResult(Activity activity) {
                        setActivity(activity);
                    }
                };

        mDelegate.trackActivityForHost(mHostId, onActivityUpdatedCallback);

        if (!mHasPermission) {
            Log.w(TAG, "NFC operations are not permitted.");
            mNfcAdapter = null;
            mNfcManager = null;
        } else {
            mNfcManager =
                    (NfcManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.NFC_SERVICE);
            if (mNfcManager == null) {
                Log.w(TAG, "NFC is not supported.");
                mNfcAdapter = null;
            } else {
                mNfcAdapter = mNfcManager.getDefaultAdapter();
            }
        }

        mVibrator =
                (Vibrator)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.VIBRATOR_SERVICE);
    }

    /**
     * Sets Activity that is used to enable / disable NFC reader mode. When Activity is set,
     * reader mode is disabled for old Activity and enabled for the new Activity.
     */
    protected void setActivity(Activity activity) {
        disableReaderMode();
        mActivity = activity;
        enableReaderModeIfNeeded();
    }

    /**
     * Forces the Mojo connection to this object to be closed. This will trigger a call to close()
     * so that pending NFC operations are canceled.
     */
    public void closeMojoConnection() {
        if (mRouter != null) {
            mRouter.close();
            mRouter = null;
        }
    }

    /**
     * Sets NfcClient. NfcClient interface is used to notify mojo NFC service client when NFC device
     * is in proximity and has NdefMessage.
     *
     * @see Nfc#watch(int id, Watch_Response callback)
     * @see NfcClient
     */
    @Override
    public void setClient(NfcClient client) {
        mClient = client;
    }

    /**
     * Pushes NdefMessage to NFC Tag whenever it is in proximity.
     *
     * @param message that should be pushed to NFC device.
     * @param options that contain options for the pending push operation.
     * @param callback that is used to notify when push operation is completed.
     */
    @Override
    public void push(NdefMessage message, NdefWriteOptions options, Push_Response callback) {
        NdefError error = checkIfReady();
        if (error != null) {
            callback.call(error);
            return;
        }

        if (mOperationsSuspended) {
            callback.call(
                    createError(
                            NdefErrorType.OPERATION_CANCELLED,
                            "Cannot push the message because NFC operations are suspended."));
        }

        if (!NdefMessageValidator.isValid(message)) {
            callback.call(
                    createError(
                            NdefErrorType.INVALID_MESSAGE,
                            "Cannot push the message because it's invalid."));
            return;
        }

        // If previous pending push operation is not completed, cancel it.
        if (mPendingPushOperation != null) {
            mPendingPushOperation.complete(
                    createError(
                            NdefErrorType.OPERATION_CANCELLED,
                            "Push is cancelled due to a new push request."));
        }

        mPendingPushOperation = new PendingPushOperation(message, options, callback);

        enableReaderModeIfNeeded();
        processPendingPushOperation();
    }

    /** Cancels pending push operation. */
    @Override
    public void cancelPush() {
        completePendingPushOperation(
                createError(NdefErrorType.OPERATION_CANCELLED, "The push operation is cancelled."));
    }

    /**
     * Make NFC tag read-only whenever it is in proximity.
     *
     * @param callback that is used to notify when make read-only operation is completed.
     */
    @Override
    public void makeReadOnly(MakeReadOnly_Response callback) {
        NdefError error = checkIfReady();
        if (error != null) {
            callback.call(error);
            return;
        }

        if (mOperationsSuspended) {
            callback.call(
                    createError(
                            NdefErrorType.OPERATION_CANCELLED,
                            "Cannot make read-only because NFC operations are suspended."));
        }

        // If previous pending make read-only operation is not completed, cancel it.
        if (mPendingMakeReadOnlyOperation != null) {
            mPendingMakeReadOnlyOperation.complete(
                    createError(
                            NdefErrorType.OPERATION_CANCELLED,
                            "Make read-only is cancelled due to a new make read-only request."));
        }

        mPendingMakeReadOnlyOperation = new PendingMakeReadOnlyOperation(callback);

        enableReaderModeIfNeeded();
        processPendingMakeReadOnlyOperation();
    }

    /** Cancels pending make read-only operation. */
    @Override
    public void cancelMakeReadOnly() {
        completePendingMakeReadOnlyOperation(
                createError(
                        NdefErrorType.OPERATION_CANCELLED,
                        "The make read-only operation is cancelled."));
    }

    /**
     * When NdefMessages that are found when NFC device is within proximity, it
     * is passed to NfcClient interface together with corresponding watch ID.
     * @see NfcClient#onWatch(int[] id, String serial_number, NdefMessage message)
     *
     * @param id request ID from Blink which will be the watch ID if succeeded.
     * @param callback that is used to notify caller when watch() is completed.
     */
    @Override
    public void watch(int id, Watch_Response callback) {
        NdefError error = checkIfReady();
        if (error != null) {
            callback.call(error);
            return;
        }

        // We received a duplicate |id| here that should never happen, in such a case we should
        // report a bad message to Mojo but unfortunately Mojo bindings for Java does not support
        // this feature yet. So, we just passes back a generic error instead.
        if (mWatchIds.contains(id)) {
            callback.call(
                    createError(
                            NdefErrorType.NOT_READABLE,
                            "Cannot start because the received scan request is duplicate."));
            return;
        }
        mWatchIds.add(id);
        callback.call(null);
        enableReaderModeIfNeeded();
        processPendingWatchOperations();
    }

    /**
     * Cancels NFC watch operation.
     *
     * @param id of watch operation.
     */
    @Override
    public void cancelWatch(int id) {
        if (mWatchIds.contains(id)) {
            mWatchIds.remove(mWatchIds.indexOf(id));
            disableReaderModeIfNeeded();
        }
    }

    @Override
    public void close() {
        mDelegate.stopTrackingActivityForHost(mHostId);
        disableReaderMode();
    }

    @Override
    public void onConnectionError(MojoException e) {
        // We do nothing here since close() is always called no matter the connection gets closed
        // normally or abnormally.
    }

    /** Suspends all pending watch operations and cancel push / makeReadOnly operations. */
    public void suspendNfcOperations() {
        mOperationsSuspended = true;
        disableReaderMode();
        cancelPush();
        cancelMakeReadOnly();
    }

    /** Resumes all pending watch / push / makeReadOnly operations. */
    public void resumeNfcOperations() {
        mOperationsSuspended = false;
        enableReaderModeIfNeeded();
    }

    /** Holds information about pending push operation. */
    private static class PendingPushOperation {
        public final NdefMessage ndefMessage;
        public final NdefWriteOptions ndefWriteOptions;
        private final Push_Response mPushResponseCallback;

        public PendingPushOperation(
                NdefMessage message, NdefWriteOptions options, Push_Response callback) {
            ndefMessage = message;
            ndefWriteOptions = options;
            mPushResponseCallback = callback;
        }

        /**
         * Completes pending push operation.
         *
         * @param error should be null when operation is completed successfully, otherwise,
         * error object with corresponding NdefErrorType should be provided.
         */
        public void complete(NdefError error) {
            if (mPushResponseCallback != null) mPushResponseCallback.call(error);
        }
    }

    /** Holds information about pending make read-only operation. */
    private static class PendingMakeReadOnlyOperation {
        private final MakeReadOnly_Response mMakeReadOnlyResponseCallback;

        public PendingMakeReadOnlyOperation(MakeReadOnly_Response callback) {
            mMakeReadOnlyResponseCallback = callback;
        }

        /**
         * Completes pending make read-only operation.
         *
         * @param error should be null when operation is completed successfully, otherwise,
         * error object with corresponding NdefErrorType should be provided.
         */
        public void complete(NdefError error) {
            if (mMakeReadOnlyResponseCallback != null) mMakeReadOnlyResponseCallback.call(error);
        }
    }

    /** Helper method that creates NdefError object from NdefErrorType. */
    private NdefError createError(int errorType, String errorMessage) {
        // Guaranteed by callers.
        assert errorMessage != null;

        NdefError error = new NdefError();
        error.errorType = errorType;
        error.errorMessage = errorMessage;
        return error;
    }

    /**
     * Checks if NFC functionality can be used by the mojo service. If permission to use NFC is
     * granted and hardware is enabled, returns null.
     */
    private NdefError checkIfReady() {
        if (!mHasPermission || mActivity == null) {
            return createError(NdefErrorType.NOT_ALLOWED, "The operation is not allowed.");
        } else if (mNfcManager == null || mNfcAdapter == null) {
            return createError(NdefErrorType.NOT_SUPPORTED, "NFC is not supported.");
        } else if (!mNfcAdapter.isEnabled()) {
            return createError(NdefErrorType.NOT_READABLE, "NFC setting is disabled.");
        }
        return null;
    }

    /**
     * Implementation of android.nfc.NfcAdapter.ReaderCallback. Callback is called when NFC tag is
     * discovered, Tag object is delegated to mojo service implementation method
     * NfcImpl.onTagDiscovered().
     */
    private static class ReaderCallbackHandler implements ReaderCallback {
        private final NfcImpl mNfcImpl;

        public ReaderCallbackHandler(NfcImpl impl) {
            mNfcImpl = impl;
        }

        @Override
        public void onTagDiscovered(Tag tag) {
            mNfcImpl.onTagDiscovered(tag);
        }
    }

    /**
     * Returns true if there are active push / makeReadOnly / watch operations. Otherwise, false.
     */
    private boolean hasActiveOperations() {
        return (mPendingPushOperation != null
                || mPendingMakeReadOnlyOperation != null
                || mWatchIds.size() != 0);
    }

    /**
     * Enables reader mode, allowing NFC device to read / write / make read-only NFC tags.
     * @see android.nfc.NfcAdapter#enableReaderMode
     */
    private void enableReaderModeIfNeeded() {
        if (mReaderCallbackHandler != null || mActivity == null || mNfcAdapter == null) return;

        if (!hasActiveOperations()) return;

        mReaderCallbackHandler = new ReaderCallbackHandler(this);
        mNfcAdapter.enableReaderMode(
                mActivity,
                mReaderCallbackHandler,
                NfcAdapter.FLAG_READER_NFC_A
                        | NfcAdapter.FLAG_READER_NFC_B
                        | NfcAdapter.FLAG_READER_NFC_F
                        | NfcAdapter.FLAG_READER_NFC_V
                        | NfcAdapter.FLAG_READER_NO_PLATFORM_SOUNDS,
                null);
    }

    /**
     * Disables reader mode.
     * @see android.nfc.NfcAdapter#disableReaderMode
     */
    private void disableReaderMode() {
        // There is no API that could query whether reader mode is enabled for adapter.
        // If mReaderCallbackHandler is null, reader mode is not enabled.
        if (mReaderCallbackHandler == null) return;

        mReaderCallbackHandler = null;
        if (mActivity != null && mNfcAdapter != null && !mActivity.isDestroyed()) {
            mNfcAdapter.disableReaderMode(mActivity);
        }
    }

    /**
     * Checks if there are pending push / makeReadOnly / watch operations and disables reader mode
     * whenever necessary.
     */
    private void disableReaderModeIfNeeded() {
        if (hasActiveOperations()) return;

        PostTask.postDelayedTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    if (!hasActiveOperations()) disableReaderMode();
                },
                DELAY_TO_DISABLE_READER_MODE_MS);
    }

    /**
     * Handles completion of pending push operation, completes push operation.
     * On error, invalidates #mTagHandler.
     */
    private void pendingPushOperationCompleted(NdefError error) {
        completePendingPushOperation(error);
        if (error != null) mTagHandler = null;
    }

    /** Completes pending push operation and disables reader mode if needed. */
    private void completePendingPushOperation(NdefError error) {
        if (mPendingPushOperation == null) return;

        mPendingPushOperation.complete(error);
        mPendingPushOperation = null;
        disableReaderModeIfNeeded();
    }

    /**
     * Checks whether there is a #mPendingPushOperation and writes data to NFC tag. In case of
     * exception calls pendingPushOperationCompleted() with appropriate error object.
     */
    private void processPendingPushOperation() {
        if (mTagHandler == null || mPendingPushOperation == null) return;

        if (mTagHandler.isTagOutOfRange()) {
            mTagHandler = null;
            return;
        }

        try {
            mTagHandler.connect();

            if (!mPendingPushOperation.ndefWriteOptions.overwrite
                    && !mTagHandler.canAlwaysOverwrite()) {
                Log.w(TAG, "Cannot overwrite the NFC tag due to existing data on it.");
                pendingPushOperationCompleted(
                        createError(
                                NdefErrorType.NOT_ALLOWED,
                                "NDEFWriteOptions#overwrite does not allow overwrite."));
                return;
            }

            mTagHandler.write(NdefMessageUtils.toNdefMessage(mPendingPushOperation.ndefMessage));
            pendingPushOperationCompleted(null);
        } catch (InvalidNdefMessageException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Invalid NdefMessage.");
            pendingPushOperationCompleted(
                    createError(
                            NdefErrorType.INVALID_MESSAGE,
                            "Cannot push the message because it's invalid."));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Tag is lost: " + e.getMessage());
            pendingPushOperationCompleted(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to write because the tag is lost: " + e.getMessage()));
        } catch (FormatException | IllegalStateException | IOException | SecurityException e) {
            Log.w(TAG, "Cannot write data to NFC tag: " + e.getMessage());
            pendingPushOperationCompleted(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to write due to an IO error: " + e.getMessage()));
        }
    }

    /**
     * Handles completion of pending make read-only operation, completes make read-only operation.
     * On error, invalidates #mTagHandler.
     */
    private void pendingMakeReadOnlyOperationCompleted(NdefError error) {
        completePendingMakeReadOnlyOperation(error);
        if (error != null) mTagHandler = null;
    }

    /** Completes pending make read-only operation and disables reader mode if needed. */
    private void completePendingMakeReadOnlyOperation(NdefError error) {
        if (mPendingMakeReadOnlyOperation == null) return;

        mPendingMakeReadOnlyOperation.complete(error);
        mPendingMakeReadOnlyOperation = null;
        disableReaderModeIfNeeded();
    }

    /**
     * Checks whether there is a #mPendingMakeReadOnlyOperation and make NFC tag read-only. In case
     * of exception calls pendingMakeReadOnlyOperationCompleted() with appropriate error object.
     */
    private void processPendingMakeReadOnlyOperation() {
        if (mTagHandler == null || mPendingMakeReadOnlyOperation == null) return;

        if (mTagHandler.isTagOutOfRange()) {
            mTagHandler = null;
            return;
        }

        try {
            mTagHandler.connect();
            if (mTagHandler.makeReadOnly()) {
                pendingMakeReadOnlyOperationCompleted(null);
            } else {
                Log.w(TAG, "Cannot make NFC tag read-only. The tag cannot be made read-only");
                pendingMakeReadOnlyOperationCompleted(
                        createError(
                                NdefErrorType.NOT_SUPPORTED,
                                "Failed to make read-only because the tag cannot be made"
                                        + " read-only"));
            }
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot make NFC tag read-only. Tag is lost: " + e.getMessage());
            pendingMakeReadOnlyOperationCompleted(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to make read-only because the tag is lost: " + e.getMessage()));
        } catch (IOException | SecurityException e) {
            Log.w(TAG, "Cannot make NFC tag read-only: " + e.getMessage());
            pendingMakeReadOnlyOperationCompleted(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to make read-only due to an IO error: " + e.getMessage()));
        }
    }

    /** Reads NdefMessage from a tag and forwards message to matching method. */
    private void processPendingWatchOperations() {
        if (mTagHandler == null
                || mClient == null
                || mWatchIds.size() == 0
                || mOperationsSuspended) {
            return;
        }

        if (mTagHandler.isTagOutOfRange()) {
            mTagHandler = null;
            return;
        }

        try {
            mTagHandler.connect();
            android.nfc.NdefMessage message = mTagHandler.read();
            if (message == null) {
                // Tag is formatted to support NDEF but does not contain a message yet.
                // Let's create one with no records so that watchers can be notified.
                NdefMessage webNdefMessage = new NdefMessage();
                webNdefMessage.data = new NdefRecord[0];
                notifyWatchers(webNdefMessage);
                return;
            }
            NdefMessage webNdefMessage = NdefMessageUtils.toNdefMessage(message);
            notifyWatchers(webNdefMessage);
        } catch (UnsupportedEncodingException e) {
            Log.w(
                    TAG,
                    "Cannot read data from NFC tag. Cannot convert to NdefMessage:"
                            + e.getMessage());
            notifyErrorToAllWatchers(
                    createError(
                            NdefErrorType.INVALID_MESSAGE,
                            "Failed to decode the NdefMessage read from the tag: "
                                    + e.getMessage()));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot read data from NFC tag. Tag is lost: " + e.getMessage());
            notifyErrorToAllWatchers(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to read because the tag is lost: " + e.getMessage()));
        } catch (FormatException | IllegalStateException | IOException | SecurityException e) {
            Log.w(TAG, "Cannot read data from NFC tag. IO_ERROR: " + e.getMessage());
            notifyErrorToAllWatchers(
                    createError(
                            NdefErrorType.IO_ERROR,
                            "Failed to read due to an IO error: " + e.getMessage()));
        }
    }

    /**
     * Notify all active watchers that an error happened when trying to read the tag coming nearby.
     */
    private void notifyErrorToAllWatchers(NdefError error) {
        if (mWatchIds.size() != 0) mClient.onError(error);
    }

    /** Iterates through active watchers and delivers NdefMessage to the client. */
    private void notifyWatchers(NdefMessage message) {
        if (mWatchIds.size() != 0) {
            int[] ids = new int[mWatchIds.size()];
            for (int i = 0; i < mWatchIds.size(); ++i) {
                ids[i] = mWatchIds.get(i).intValue();
            }
            mClient.onWatch(ids, mTagHandler.serialNumber(), message);
        }
    }

    /** Called by ReaderCallbackHandler when NFC tag is in proximity. */
    public void onTagDiscovered(Tag tag) {
        long now = System.currentTimeMillis();
        // Ensure that excessive vibration is prevented during consecutive NFC operations.
        if (now - mTagDiscoveredLastTimeMs > MIN_TIME_BETWEEN_VIBRATIONS_MS) {
            mVibrator.vibrate(200);
        }
        mTagDiscoveredLastTimeMs = now;
        processPendingOperations(NfcTagHandler.create(tag));
    }

    /** Processes pending operation when NFC tag is in proximity. */
    protected void processPendingOperations(NfcTagHandler tagHandler) {
        mTagHandler = tagHandler;

        // This tag is not supported.
        if (mTagHandler == null) {
            Log.w(TAG, "This tag is not supported.");
            notifyErrorToAllWatchers(
                    createError(NdefErrorType.NOT_SUPPORTED, "This tag is not supported."));
            pendingPushOperationCompleted(
                    createError(NdefErrorType.NOT_SUPPORTED, "This tag is not supported."));
            pendingMakeReadOnlyOperationCompleted(
                    createError(NdefErrorType.NOT_SUPPORTED, "This tag is not supported."));
            return;
        }

        processPendingWatchOperations();
        processPendingPushOperation();
        processPendingMakeReadOnlyOperation();
    }
}
