// Copyright 2016 The Chromium Authors. All rights reserved.
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
import org.chromium.device.mojom.NdefError;
import org.chromium.device.mojom.NdefErrorType;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefWriteOptions;
import org.chromium.device.mojom.Nfc;
import org.chromium.device.mojom.NfcClient;
import org.chromium.mojo.bindings.Callbacks;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.bindings.Router;
import org.chromium.mojo.system.MojoException;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

/** Android implementation of the NFC mojo service defined in device/nfc/nfc.mojom.
 */
public class NfcImpl implements Nfc {
    private static final String TAG = "NfcImpl";
    private static final String ANY_PATH = "/*";

    private final int mHostId;

    private final NfcDelegate mDelegate;

    private Router mRouter;

    /**
     * Used to get instance of NFC adapter, @see android.nfc.NfcManager
     */
    private final NfcManager mNfcManager;

    /**
     * NFC adapter. @see android.nfc.NfcAdapter
     */
    private final NfcAdapter mNfcAdapter;

    /**
     * Activity that is in foreground and is used to enable / disable NFC reader mode operations.
     * Can be updated when activity associated with web page is changed. @see #setActivity
     */
    private Activity mActivity;

    /**
     * Flag that indicates whether NFC permission is granted.
     */
    private final boolean mHasPermission;

    /**
     * Implementation of android.nfc.NfcAdapter.ReaderCallback. @see ReaderCallbackHandler
     */
    private ReaderCallbackHandler mReaderCallbackHandler;

    /**
     * Object that contains data that was passed to method
     * #push(NdefMessage message, NdefWriteOptions options, PushResponse callback)
     * @see PendingPushOperation
     */
    private PendingPushOperation mPendingPushOperation;

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
    /**
     * Vibrator. @see android.os.Vibrator
     */
    private Vibrator mVibrator;

    public NfcImpl(int hostId, NfcDelegate delegate, InterfaceRequest<Nfc> request) {
        mHostId = hostId;
        mDelegate = delegate;

        // |request| may be null in tests.
        if (request != null) {
            mRouter = Nfc.MANAGER.bind(this, request);
        }

        int permission = ContextUtils.getApplicationContext().checkPermission(
                Manifest.permission.NFC, Process.myPid(), Process.myUid());
        mHasPermission = permission == PackageManager.PERMISSION_GRANTED;
        Callback<Activity> onActivityUpdatedCallback = new Callback<Activity>() {
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
            mNfcManager = (NfcManager) ContextUtils.getApplicationContext().getSystemService(
                    Context.NFC_SERVICE);
            if (mNfcManager == null) {
                Log.w(TAG, "NFC is not supported.");
                mNfcAdapter = null;
            } else {
                mNfcAdapter = mNfcManager.getDefaultAdapter();
            }
        }

        mVibrator = (Vibrator) ContextUtils.getApplicationContext().getSystemService(
                Context.VIBRATOR_SERVICE);
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
     * Sets NfcClient. NfcClient interface is used to notify mojo NFC service client when NFC
     * device is in proximity and has NdefMessage.
     * @see Nfc#watch(int id, WatchResponse callback)
     *
     * @param client @see NfcClient
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
    public void push(NdefMessage message, NdefWriteOptions options, PushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (!NdefMessageValidator.isValid(message)) {
            callback.call(createError(NdefErrorType.INVALID_MESSAGE,
                    "Cannot push the message because it's invalid."));
            return;
        }

        // If previous pending push operation is not completed, cancel it.
        if (mPendingPushOperation != null) {
            mPendingPushOperation.complete(createError(NdefErrorType.OPERATION_CANCELLED,
                    "Push is cancelled due to a new push request."));
        }

        mPendingPushOperation = new PendingPushOperation(message, options, callback);

        enableReaderModeIfNeeded();
        processPendingPushOperation();
    }

    /**
     * Cancels pending push operation.
     *
     * @param callback that is used to notify caller when cancelPush() is completed.
     */
    @Override
    public void cancelPush(CancelPushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (mPendingPushOperation == null) {
            callback.call(createError(
                    NdefErrorType.CANNOT_CANCEL, "No pending push operation to cancel."));
        } else {
            completePendingPushOperation(createError(
                    NdefErrorType.OPERATION_CANCELLED, "The push operation is already cancelled."));
            callback.call(null);
        }
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
    public void watch(int id, WatchResponse callback) {
        if (!checkIfReady(callback)) return;
        // We received a duplicate |id| here that should never happen, in such a case we should
        // report a bad message to Mojo but unfortunately Mojo bindings for Java does not support
        // this feature yet. So, we just passes back a generic error instead.
        if (mWatchIds.contains(id)) {
            callback.call(createError(NdefErrorType.NOT_READABLE,
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
     * @param callback that is used to notify caller when cancelWatch() is completed.
     */
    @Override
    public void cancelWatch(int id, CancelWatchResponse callback) {
        if (!checkIfReady(callback)) return;

        if (!mWatchIds.contains(id)) {
            callback.call(
                    createError(NdefErrorType.NOT_FOUND, "No pending scan operation to cancel."));
        } else {
            mWatchIds.remove(mWatchIds.indexOf(id));
            callback.call(null);
            disableReaderModeIfNeeded();
        }
    }

    /**
     * Cancels all NFC watch operations.
     *
     * @param callback that is used to notify caller when cancelAllWatches() is completed.
     */
    @Override
    public void cancelAllWatches(CancelAllWatchesResponse callback) {
        if (!checkIfReady(callback)) return;

        if (mWatchIds.size() == 0) {
            callback.call(
                    createError(NdefErrorType.NOT_FOUND, "No pending scan operation to cancel."));
        } else {
            mWatchIds.clear();
            callback.call(null);
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

    /**
     * Suspends all pending operations.
     */
    public void suspendNfcOperations() {
        disableReaderMode();
    }

    /**
     * Resumes all pending watch / push operations.
     */
    public void resumeNfcOperations() {
        enableReaderModeIfNeeded();
    }

    /**
     * Holds information about pending push operation.
     */
    private static class PendingPushOperation {
        public final NdefMessage ndefMessage;
        public final NdefWriteOptions ndefWriteOptions;
        private final PushResponse mPushResponseCallback;

        public PendingPushOperation(
                NdefMessage message, NdefWriteOptions options, PushResponse callback) {
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

    /**
     * Helper method that creates NdefError object from NdefErrorType.
     */
    private NdefError createError(int errorType, String errorMessage) {
        // Guaranteed by callers.
        assert errorMessage != null;

        NdefError error = new NdefError();
        error.errorType = errorType;
        error.errorMessage = errorMessage;
        return error;
    }

    /**
     * Checks if NFC funcionality can be used by the mojo service. If permission to use NFC is
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
     * Uses checkIfReady() method and if NFC cannot be used, calls mojo callback with NdefError.
     *
     * @param WatchResponse Callback that is provided to watch() method.
     * @return boolean true if NFC functionality can be used, false otherwise.
     */
    private boolean checkIfReady(WatchResponse callback) {
        NdefError error = checkIfReady();
        if (error == null) return true;

        callback.call(error);
        return false;
    }

    /**
     * Uses checkIfReady() method and if NFC cannot be used, calls mojo callback with NdefError.
     *
     * @param callback Generic callback that is provided to push(), cancelPush(),
     * cancelWatch() and cancelAllWatches() methods.
     * @return boolean true if NFC functionality can be used, false otherwise.
     */
    private boolean checkIfReady(Callbacks.Callback1<NdefError> callback) {
        NdefError error = checkIfReady();
        if (error == null) return true;

        callback.call(error);
        return false;
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
     * Enables reader mode, allowing NFC device to read / write NFC tags.
     * @see android.nfc.NfcAdapter#enableReaderMode
     */
    private void enableReaderModeIfNeeded() {
        if (mReaderCallbackHandler != null || mActivity == null || mNfcAdapter == null) return;

        // Do not enable reader mode, if there are no active push / watch operations.
        if (mPendingPushOperation == null && mWatchIds.size() == 0) return;

        mReaderCallbackHandler = new ReaderCallbackHandler(this);
        mNfcAdapter.enableReaderMode(mActivity, mReaderCallbackHandler,
                NfcAdapter.FLAG_READER_NFC_A | NfcAdapter.FLAG_READER_NFC_B
                        | NfcAdapter.FLAG_READER_NFC_F | NfcAdapter.FLAG_READER_NFC_V
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
     * Checks if there are pending push / watch operations and disables readre mode
     * whenever necessary.
     */
    private void disableReaderModeIfNeeded() {
        if (mPendingPushOperation == null && mWatchIds.size() == 0) {
            disableReaderMode();
        }
    }

    /**
     * Handles completion of pending push operation, completes push operation.
     * On error, invalidates #mTagHandler.
     */
    private void pendingPushOperationCompleted(NdefError error) {
        completePendingPushOperation(error);
        if (error != null) mTagHandler = null;
    }

    /**
     * Completes pending push operation and disables reader mode if needed.
     */
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
                pendingPushOperationCompleted(createError(NdefErrorType.NOT_ALLOWED,
                        "NDEFWriteOptions#overwrite does not allow overwrite."));
                return;
            }

            mTagHandler.write(NdefMessageUtils.toNdefMessage(mPendingPushOperation.ndefMessage));
            pendingPushOperationCompleted(null);
        } catch (InvalidNdefMessageException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Invalid NdefMessage.");
            pendingPushOperationCompleted(createError(NdefErrorType.INVALID_MESSAGE,
                    "Cannot push the message because it's invalid."));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Tag is lost: " + e.getMessage());
            pendingPushOperationCompleted(createError(NdefErrorType.IO_ERROR,
                    "Failed to write because the tag is lost: " + e.getMessage()));
        } catch (FormatException | IllegalStateException | IOException e) {
            Log.w(TAG, "Cannot write data to NFC tag: " + e.getMessage());
            pendingPushOperationCompleted(createError(NdefErrorType.IO_ERROR,
                    "Failed to write due to an IO error: " + e.getMessage()));
        }
    }

    /**
     * Reads NdefMessage from a tag and forwards message to matching method.
     */
    private void processPendingWatchOperations() {
        if (mTagHandler == null || mClient == null || mWatchIds.size() == 0) return;

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
            Log.w(TAG,
                    "Cannot read data from NFC tag. Cannot convert to NdefMessage:"
                            + e.getMessage());
            notifyErrorToAllWatchers(createError(NdefErrorType.INVALID_MESSAGE,
                    "Failed to decode the NdefMessage read from the tag: " + e.getMessage()));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot read data from NFC tag. Tag is lost: " + e.getMessage());
            notifyErrorToAllWatchers(createError(NdefErrorType.IO_ERROR,
                    "Failed to read because the tag is lost: " + e.getMessage()));
        } catch (FormatException | IllegalStateException | IOException e) {
            Log.w(TAG, "Cannot read data from NFC tag. IO_ERROR: " + e.getMessage());
            notifyErrorToAllWatchers(createError(NdefErrorType.IO_ERROR,
                    "Failed to read due to an IO error: " + e.getMessage()));
        }
    }

    /**
     * Notify all active watchers that an error happened when trying to read the tag coming nearby.
     */
    private void notifyErrorToAllWatchers(NdefError error) {
        for (int i = 0; i < mWatchIds.size(); i++) {
            mClient.onError(error);
        }
    }

    /**
     * Iterates through active watchers and delivers NdefMessage to the client.
     */
    private void notifyWatchers(NdefMessage message) {
        if (mWatchIds.size() != 0) {
            int[] ids = new int[mWatchIds.size()];
            for (int i = 0; i < mWatchIds.size(); ++i) {
                ids[i] = mWatchIds.get(i).intValue();
            }
            mClient.onWatch(ids, mTagHandler.serialNumber(), message);
        }
    }

    /**
     * Called by ReaderCallbackHandler when NFC tag is in proximity.
     */
    public void onTagDiscovered(Tag tag) {
        mVibrator.vibrate(200);
        processPendingOperations(NfcTagHandler.create(tag));
    }

    /**
     * Processes pending operation when NFC tag is in proximity.
     */
    protected void processPendingOperations(NfcTagHandler tagHandler) {
        mTagHandler = tagHandler;

        // This tag is not supported.
        if (mTagHandler == null) {
            Log.w(TAG, "This tag is not supported.");
            notifyErrorToAllWatchers(
                    createError(NdefErrorType.NOT_SUPPORTED, "This tag is not supported."));
            pendingPushOperationCompleted(
                    createError(NdefErrorType.NOT_SUPPORTED, "This tag is not supported."));
            return;
        }

        processPendingWatchOperations();
        processPendingPushOperation();
        if (mTagHandler != null && mTagHandler.isConnected()) {
            try {
                mTagHandler.close();
            } catch (IOException e) {
                Log.w(TAG, "Cannot close NFC tag connection.");
            }
        }
    }
}
