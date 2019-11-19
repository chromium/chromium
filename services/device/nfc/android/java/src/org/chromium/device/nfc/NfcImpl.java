// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.os.Build;
import android.os.Process;
import android.os.Vibrator;
import android.util.SparseArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.device.mojom.NdefError;
import org.chromium.device.mojom.NdefErrorType;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefPushOptions;
import org.chromium.device.mojom.NdefPushTarget;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefScanOptions;
import org.chromium.device.mojom.Nfc;
import org.chromium.device.mojom.NfcClient;
import org.chromium.mojo.bindings.Callbacks;
import org.chromium.mojo.system.MojoException;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/** Android implementation of the NFC mojo service defined in device/nfc/nfc.mojom.
 */
public class NfcImpl implements Nfc {
    private static final String TAG = "NfcImpl";
    private static final String ANY_PATH = "/*";

    private final int mHostId;

    private final NfcDelegate mDelegate;

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
     * #push(NdefMessage message, NdefPushOptions options, PushResponse callback)
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

    /**
     * Watcher id that is incremented for each #watch call.
     */
    private int mWatcherId;

    /**
     * Map of watchId <-> NdefScanOptions. All NdefScanOptions are matched against tag that is in
     * proximity, when match algorithm (@see #matchesWatchOptions) returns true, watcher with
     * corresponding ID would be notified using NfcClient interface.
     * @see NfcClient#onWatch(int[] id, String serial_number, NdefMessage message)
     */
    private final SparseArray<NdefScanOptions> mWatchers = new SparseArray<>();

    /**
     * Vibrator. @see android.os.Vibrator
     */
    private Vibrator mVibrator;

    public NfcImpl(int hostId, NfcDelegate delegate) {
        mHostId = hostId;
        mDelegate = delegate;
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

        if (!mHasPermission || Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
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
     * Sets NfcClient. NfcClient interface is used to notify mojo NFC service client when NFC
     * device is in proximity and has NdefMessage that matches NdefScanOptions criteria.
     * @see Nfc#watch(NdefScanOptions options, int id, WatchResponse callback)
     *
     * @param client @see NfcClient
     */
    @Override
    public void setClient(NfcClient client) {
        mClient = client;
    }

    /**
     * Pushes NdefMessage to Tag or Peer, whenever NFC device is in proximity. At the moment, only
     * passive NFC devices are supported (NdefPushTarget.TAG).
     *
     * @param message that should be pushed to NFC device.
     * @param options that contain information about target device type.
     * @param callback that is used to notify when push operation is completed.
     */
    @Override
    public void push(NdefMessage message, NdefPushOptions options, PushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (!NdefMessageValidator.isValid(message)) {
            callback.call(createError(NdefErrorType.INVALID_MESSAGE));
            return;
        }

        // Check NdefPushOptions that are not supported by Android platform.
        if (options.target == NdefPushTarget.PEER) {
            callback.call(createError(NdefErrorType.NOT_SUPPORTED));
            return;
        }

        // If previous pending push operation is not completed, cancel it.
        if (mPendingPushOperation != null) {
            mPendingPushOperation.complete(createError(NdefErrorType.OPERATION_CANCELLED));
        }

        mPendingPushOperation = new PendingPushOperation(message, options, callback);

        enableReaderModeIfNeeded();
        processPendingPushOperation();
    }

    /**
     * Cancels pending push operation.
     * At the moment, only passive NFC devices are supported (NdefPushTarget.TAG).
     *
     * @param target @see NdefPushTarget
     * @param callback that is used to notify caller when cancelPush() is completed.
     */
    @Override
    public void cancelPush(int target, CancelPushResponse callback) {
        if (!checkIfReady(callback)) return;

        if (target == NdefPushTarget.PEER) {
            callback.call(createError(NdefErrorType.NOT_SUPPORTED));
            return;
        }

        if (mPendingPushOperation == null) {
            callback.call(createError(NdefErrorType.CANNOT_CANCEL));
        } else {
            completePendingPushOperation(createError(NdefErrorType.OPERATION_CANCELLED));
            callback.call(null);
        }
    }

    /**
     * Watch method allows to set filtering criteria for NdefMessages that are found when NFC device
     * is within proximity. When NdefMessage that matches NdefScanOptions is found, it is passed to
     * NfcClient interface together with corresponding watch ID.
     * @see NfcClient#onWatch(int[] id, String serial_number, NdefMessage message)
     *
     * @param options used to filter NdefMessages, @see NdefScanOptions.
     * @param callback that is used to notify caller when watch() is completed.
     */
    @Override
    public void watch(NdefScanOptions options, int id, WatchResponse callback) {
        if (!checkIfReady(callback)) return;
        // We received a duplicate |id| here that should never happen, in such a case we should
        // report a bad message to Mojo but unfortunately Mojo bindings for Java does not support
        // this feature yet. So, we just passes back a generic error instead.
        if (mWatchers.indexOfKey(id) >= 0) {
            callback.call(createError(NdefErrorType.NOT_READABLE));
            return;
        }
        mWatchers.put(id, options);
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

        if (mWatchers.indexOfKey(id) < 0) {
            callback.call(createError(NdefErrorType.NOT_FOUND));
        } else {
            mWatchers.remove(id);
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

        if (mWatchers.size() == 0) {
            callback.call(createError(NdefErrorType.NOT_FOUND));
        } else {
            mWatchers.clear();
            callback.call(null);
            disableReaderModeIfNeeded();
        }
    }

    /**
     * Suspends all pending operations. Should be called when web page visibility is lost.
     */
    @Override
    public void suspendNfcOperations() {
        disableReaderMode();
    }

    /**
     * Resumes all pending watch / push operations. Should be called when web page becomes visible.
     */
    @Override
    public void resumeNfcOperations() {
        enableReaderModeIfNeeded();
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
     * Holds information about pending push operation.
     */
    private static class PendingPushOperation {
        public final NdefMessage ndefMessage;
        public final NdefPushOptions ndefPushOptions;
        private final PushResponse mPushResponseCallback;

        public PendingPushOperation(
                NdefMessage message, NdefPushOptions options, PushResponse callback) {
            ndefMessage = message;
            ndefPushOptions = options;
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
    private NdefError createError(int errorType) {
        NdefError error = new NdefError();
        error.errorType = errorType;
        return error;
    }

    /**
     * Checks if NFC funcionality can be used by the mojo service. If permission to use NFC is
     * granted and hardware is enabled, returns null.
     */
    private NdefError checkIfReady() {
        if (!mHasPermission || mActivity == null) {
            return createError(NdefErrorType.NOT_ALLOWED);
        } else if (mNfcManager == null || mNfcAdapter == null) {
            return createError(NdefErrorType.NOT_SUPPORTED);
        } else if (!mNfcAdapter.isEnabled()) {
            return createError(NdefErrorType.NOT_READABLE);
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
    @TargetApi(Build.VERSION_CODES.KITKAT)
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
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        if (mReaderCallbackHandler != null || mActivity == null || mNfcAdapter == null) return;

        // Do not enable reader mode, if there are no active push / watch operations.
        if (mPendingPushOperation == null && mWatchers.size() == 0) return;

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
    @TargetApi(Build.VERSION_CODES.KITKAT)
    private void disableReaderMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

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
        if (mPendingPushOperation == null && mWatchers.size() == 0) {
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
            mTagHandler.write(NdefMessageUtils.toNdefMessage(mPendingPushOperation.ndefMessage));
            pendingPushOperationCompleted(null);
        } catch (InvalidNdefMessageException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Invalid NdefMessage.");
            pendingPushOperationCompleted(createError(NdefErrorType.INVALID_MESSAGE));
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot write data to NFC tag. Tag is lost.");
            pendingPushOperationCompleted(createError(NdefErrorType.IO_ERROR));
        } catch (FormatException | IllegalStateException | IOException e) {
            Log.w(TAG, "Cannot write data to NFC tag. IO_ERROR.");
            pendingPushOperationCompleted(createError(NdefErrorType.IO_ERROR));
        }
    }

    /**
     * Reads NdefMessage from a tag and forwards message to matching method.
     */
    private void processPendingWatchOperations() {
        if (mTagHandler == null || mClient == null || mWatchers.size() == 0) return;

        // Skip reading if there is a pending push operation and ignoreRead flag is set.
        if (mPendingPushOperation != null && mPendingPushOperation.ndefPushOptions.ignoreRead) {
            return;
        }

        if (mTagHandler.isTagOutOfRange()) {
            mTagHandler = null;
            return;
        }

        android.nfc.NdefMessage message = null;

        try {
            mTagHandler.connect();
            message = mTagHandler.read();
            if (message == null) {
                // Tag is formatted to support NDEF but does not contain a message yet.
                // Let's create one with no records so that watchers can be notified.
                NdefMessage webNdefMessage = new NdefMessage();
                webNdefMessage.data = new NdefRecord[0];
                notifyMatchingWatchers(webNdefMessage);
                return;
            }
            if (message.getByteArrayLength() > NdefMessage.MAX_SIZE) {
                Log.w(TAG, "Cannot read data from NFC tag. NdefMessage exceeds allowed size.");
                return;
            }
            NdefMessage webNdefMessage = NdefMessageUtils.toNdefMessage(message);
            notifyMatchingWatchers(webNdefMessage);
        } catch (UnsupportedEncodingException e) {
            Log.w(TAG, "Cannot read data from NFC tag. Cannot convert to NdefMessage.");
        } catch (TagLostException e) {
            Log.w(TAG, "Cannot read data from NFC tag. Tag is lost.");
        } catch (FormatException | IllegalStateException | IOException e) {
            Log.w(TAG, "Cannot read data from NFC tag. IO_ERROR.");
        }
    }

    /**
     * Iterates through active watchers and if any of those match NdefScanOptions criteria,
     * delivers NdefMessage to the client.
     */
    private void notifyMatchingWatchers(NdefMessage message) {
        List<Integer> watchIds = new ArrayList<Integer>();
        for (int i = 0; i < mWatchers.size(); i++) {
            NdefScanOptions options = mWatchers.valueAt(i);
            if (matchesWatchOptions(message, options)) {
                watchIds.add(mWatchers.keyAt(i));
            }
        }

        if (watchIds.size() != 0) {
            int[] ids = new int[watchIds.size()];
            for (int i = 0; i < watchIds.size(); ++i) {
                ids[i] = watchIds.get(i).intValue();
            }
            mClient.onWatch(ids, mTagHandler.serialNumber(), message);
        }
    }

    /**
     * Implements matching algorithm.
     */
    private boolean matchesWatchOptions(NdefMessage message, NdefScanOptions options) {
        // Filter by WebNfc watch Id.
        if (!matchesWebNfcId(message.url, options.url)) return false;

        // Matches any record / media type.
        if ((options.mediaType == null || options.mediaType.isEmpty())
                && options.recordType == null) {
            return true;
        }

        // Filter by mediaType and recordType
        for (int i = 0; i < message.data.length; i++) {
            boolean matchedMediaType;
            boolean matchedRecordType;

            if (options.mediaType == null || options.mediaType.isEmpty()) {
                // If media type for the watch options is empty, match all media types.
                matchedMediaType = true;
            } else {
                matchedMediaType = options.mediaType.equals(message.data[i].mediaType);
            }

            if (options.recordType == null) {
                // If record type for the watch options is null, match all record types.
                matchedRecordType = true;
            } else {
                matchedRecordType = options.recordType.equals(message.data[i].recordType);
            }

            if (matchedMediaType && matchedRecordType) return true;
        }

        return false;
    }

    /**
     * WebNfc Id match algorithm.
     * https://w3c.github.io/web-nfc/#url-pattern-match-algorithm
     */
    private boolean matchesWebNfcId(String id, String pattern) {
        if (id != null && !id.isEmpty() && pattern != null && !pattern.isEmpty()) {
            try {
                URL id_url = new URL(id);
                URL pattern_url = new URL(pattern);

                if (!id_url.getProtocol().equals(pattern_url.getProtocol())) return false;
                if (!id_url.getHost().endsWith("." + pattern_url.getHost())
                        && !id_url.getHost().equals(pattern_url.getHost())) {
                    return false;
                }
                if (pattern_url.getPath().equals(ANY_PATH)) return true;
                if (id_url.getPath().startsWith(pattern_url.getPath())) return true;
                return false;

            } catch (MalformedURLException e) {
                return false;
            }
        }

        return true;
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
