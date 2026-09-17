// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.printing.PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper;
import org.chromium.printing.PrintDocumentAdapterWrapper.PdfGenerator;
import org.chromium.printing.PrintDocumentAdapterWrapper.WriteResultCallbackWrapper;
import org.chromium.ui.base.WindowAndroid;

import java.io.IOException;

/**
 * Controls the interactions with Android framework related to printing.
 *
 * <p>This class is scoped to a {@link WindowAndroid} via {@link UnownedUserDataKey}. It manages the
 * printing dialog and interactions with the native side.
 *
 * <p>Key characteristics:
 *
 * <ul>
 *   <li><b>Per-Window:</b> Each browser window (WindowAndroid) has its own instance of this
 *       controller.
 *   <li><b>Lifecycle:</b> Instances are created on demand when first accessed. Because they
 *       register as an {@link WindowAndroid.ActivityStateObserver}, they are kept alive strongly by
 *       the {@link WindowAndroid} until it is destroyed. Upon destruction, they are detached from
 *       the host and cleaned up.
 *   <li><b>Busy State:</b> Tracks whether a print job is currently active for this window to
 *       prevent re-entrancy or concurrent conflicting print jobs within the same window.
 *   <li><b>Thread Safety:</b> Designed to be used on the UI thread.
 * </ul>
 *
 * <p>Usage:
 *
 * <pre>
 * PrintingController controller = PrintingControllerImpl.getInstance(window);
 * if (controller != null && !controller.isBusy()) {
 *     controller.startPrint(...);
 * }
 * </pre>
 */
@NullMarked
public class PrintingControllerImpl
        implements PrintingController, PdfGenerator, WindowAndroid.ActivityStateObserver {
    private static final String TAG = "printing";
    private static final UnownedUserDataKey<PrintingControllerImpl> KEY =
            new UnownedUserDataKey<PrintingControllerImpl>(
                    PrintingControllerImpl::onDetachedFromHost);

    /** Constant for invalid file descriptor- equivalent to base::kInvalidFd (-1) in C++. */
    public static final int INVALID_FD = -1;

    private final WindowAndroid mWindowAndroid;
    private @Nullable PrintSession mCurrentSession;

    /**
     * Stashes a pending print completion callback to be forwarded to the next created {@link
     * PrintSession}.
     */
    private @Nullable Runnable mPendingPrintCallback;

    /**
     * Sets a test instance for a specific window.
     *
     * @param window The window to attach the test instance to.
     * @param instance The test instance.
     */
    public static void setPrintingControllerForTesting(
            WindowAndroid window, PrintingControllerImpl instance) {
        UnownedUserDataHost host = window.getUnownedUserDataHost();
        KEY.attachToHost(host, instance);
        ResettersForTesting.register(() -> KEY.detachFromHost(host));
    }

    /**
     * Retrieves the {@link PrintingController} associated with the given {@link WindowAndroid}. If
     * no instance exists, one is created and attached.
     *
     * @param window The window to get the controller for.
     * @return The controller instance.
     */
    public static PrintingController getInstance(WindowAndroid window) {
        ThreadUtils.assertOnUiThread();
        UnownedUserDataHost host = window.getUnownedUserDataHost();
        PrintingControllerImpl controller = KEY.retrieveDataFromHost(host);
        if (controller == null) {
            controller = new PrintingControllerImpl(window);
            KEY.attachToHost(host, controller);
        }
        return controller;
    }

    @VisibleForTesting
    protected PrintingControllerImpl(WindowAndroid window) {
        mWindowAndroid = window;
        mWindowAndroid.addActivityStateObserver(this);
    }

    public void onDetachedFromHost(UnownedUserDataHost host) {
        mWindowAndroid.removeActivityStateObserver(this);
        notifyPendingPrintFailed();
        if (sOnDetachCallbackForTesting != null) sOnDetachCallbackForTesting.run();
    }

    private static @Nullable Runnable sOnDetachCallbackForTesting;

    /**
     * Sets a callback to be invoked when {@link #onDetachedFromHost(UnownedUserDataHost)} is
     * called.
     *
     * @param callback The callback to run.
     */
    public static void setOnDetachCallbackForTesting(Runnable callback) {
        sOnDetachCallbackForTesting = callback;
        ResettersForTesting.register(() -> sOnDetachCallbackForTesting = null);
    }

    @Override
    public void onActivityDestroyed() {
        // If the activity is destroyed, ensure we clean up any pending print jobs to avoid leaks or
        // crashes.
        notifyPendingPrintFailed();
    }

    @Override
    public boolean hasPrintingFinished() {
        return mCurrentSession != null && mCurrentSession.hasPrintingFinished();
    }

    @Override
    public int getDpi() {
        return mCurrentSession != null ? mCurrentSession.getDpi() : 0;
    }

    @Override
    public @Nullable ParcelFileDescriptor getParcelFileDescriptor() {
        return mCurrentSession != null ? mCurrentSession.getFileDescriptor() : null;
    }

    @Override
    public int getPageHeight() {
        return mCurrentSession != null ? mCurrentSession.getPageHeight() : 0;
    }

    @Override
    public int getPageWidth() {
        return mCurrentSession != null ? mCurrentSession.getPageWidth() : 0;
    }

    @Override
    public int @Nullable [] getPageNumbers() {
        return mCurrentSession != null ? mCurrentSession.getPageNumbers() : null;
    }

    @Override
    public boolean isBusy() {
        return mCurrentSession != null && mCurrentSession.isBusy();
    }

    @Override
    public void setPendingPrintCallback(Runnable callback) {
        if (mCurrentSession != null && !mCurrentSession.hasPrintingFinished()) {
            mCurrentSession.setPendingPrintCallback(callback);
        } else {
            mPendingPrintCallback = callback;
        }
    }

    @VisibleForTesting
    public @Nullable Printable getPrintable() {
        return mCurrentSession != null ? mCurrentSession.getPrintable() : null;
    }

    @Override
    public void setPendingPrint(
            final Printable printable,
            PrintManagerDelegate printManager,
            int renderProcessId,
            int renderFrameId) {
        if (isBusy()) {
            Log.d(TAG, "Pending print can't be set. PrintingController is busy.");
            return;
        }
        if (mCurrentSession != null) {
            mCurrentSession.destroy();
        }
        mCurrentSession = new PrintSession(printable, printManager, renderProcessId, renderFrameId);
        if (mPendingPrintCallback != null) {
            mCurrentSession.setPendingPrintCallback(mPendingPrintCallback);
            mPendingPrintCallback = null;
        }
    }

    @Override
    public void startPendingPrint() {
        if (isBusy()) {
            Log.d(TAG, "Pending print can't be started. PrintingController is busy.");
            return;
        }

        if (mCurrentSession == null) {
            Log.d(TAG, "Pending print can't be started. No pending print session.");
            notifyPendingPrintFailed();
            return;
        }

        if (!mCurrentSession.start(mWindowAndroid)) {
            notifyPendingPrintFailed();
        }
    }

    private void notifyPendingPrintFailed() {
        if (mCurrentSession != null) {
            mCurrentSession.destroy();
        }
        if (mPendingPrintCallback != null) {
            Runnable callback = mPendingPrintCallback;
            mPendingPrintCallback = null;
            callback.run();
        }
    }

    @Override
    public void startPrint(final Printable printable, PrintManagerDelegate printManager) {
        if (isBusy()) return;
        setPendingPrint(printable, printManager, -1, -1);
        startPendingPrint();
    }

    @Override
    public void pdfWritingDone(int pageCount) {
        if (mCurrentSession != null) {
            mCurrentSession.pdfWritingDone(pageCount);
        }
    }

    @Override
    public void onStart() {
        if (mCurrentSession != null) {
            mCurrentSession.onStart();
        }
    }

    @Override
    public void onLayout(
            PrintAttributes oldAttributes,
            PrintAttributes newAttributes,
            CancellationSignal cancellationSignal,
            LayoutResultCallbackWrapper callback,
            @Nullable Bundle metadata) {
        if (mCurrentSession != null) {
            mCurrentSession.onLayout(
                    oldAttributes, newAttributes, cancellationSignal, callback, metadata);
        } else {
            callback.onLayoutFailed(null);
        }
    }

    @Override
    public void onWrite(
            final PageRange[] ranges,
            final ParcelFileDescriptor destination,
            final CancellationSignal cancellationSignal,
            final WriteResultCallbackWrapper callback) {
        if (mCurrentSession != null) {
            mCurrentSession.onWrite(ranges, destination, cancellationSignal, callback);
        } else {
            callback.onWriteFailed(null);
            try {
                destination.close();
            } catch (IOException e) {
                /* ignore */
            }
        }
    }

    @Override
    public void onFinish() {
        if (mCurrentSession != null) {
            mCurrentSession.onFinish();
        }
        if (mPendingPrintCallback != null) {
            Runnable callback = mPendingPrintCallback;
            mPendingPrintCallback = null;
            callback.run();
        }
    }
}
