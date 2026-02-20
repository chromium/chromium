// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.printing.PrintDocumentAdapterWrapper.PdfGenerator;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Iterator;

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

    /**
     * This is used for both initial state and a completed state (i.e. starting from either onLayout
     * or onWrite, a PDF generation cycle is completed another new one can safely start).
     */
    private static final int PRINTING_STATE_READY = 0;

    private static final int PRINTING_STATE_STARTED_FROM_ONWRITE = 1;

    /** Printing dialog has been dismissed and cleanup has been done. */
    private static final int PRINTING_STATE_FINISHED = 2;

    private static final int BUFFER_SIZE = 8 * 1024; // 8 KB

    private @Nullable String mErrorMessage;

    private int mRenderProcessId;
    private int mRenderFrameId;

    /** The file descriptor into which the PDF will be written. Provided by the framework. */
    private @Nullable ParcelFileDescriptor mFileDescriptor;

    /** Dots per inch, as provided by the framework. */
    private int mDpi;

    /** Paper dimensions. */
    private PrintAttributes.@Nullable MediaSize mMediaSize;

    /** Numbers of pages to be printed, zero indexed. */
    private int @Nullable [] mPages;

    /** The callback function to inform the result of PDF generation to the framework. */
    private PrintDocumentAdapterWrapper.@Nullable WriteResultCallbackWrapper mOnWriteCallback;

    /**
     * The callback function to inform the result of layout to the framework.  We save the callback
     * because we start the native PDF generation process inside onLayout, and we need to pass the
     * number of expected pages back to the framework through this callback once the native side
     * has that information.
     */
    private PrintDocumentAdapterWrapper.@Nullable LayoutResultCallbackWrapper mOnLayoutCallback;

    /** The object through which native PDF generation process is initiated. */
    private @Nullable Printable mPrintable;

    /** The object through which the framework will make calls for generating PDF. */
    private final PrintDocumentAdapterWrapper mPrintDocumentAdapterWrapper;

    private int mPrintingState = PRINTING_STATE_READY;

    private boolean mIsBusy;

    private @Nullable PrintManagerDelegate mPrintManager;

    private final WindowAndroid mWindowAndroid;

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
        mPrintDocumentAdapterWrapper = new PrintDocumentAdapterWrapper(this);
        mWindowAndroid = window;
        mWindowAndroid.addActivityStateObserver(this);
    }

    public void onDetachedFromHost(UnownedUserDataHost host) {
        mWindowAndroid.removeActivityStateObserver(this);
        mIsBusy = false;
        mPrintingState = PRINTING_STATE_FINISHED;
        closeFileDescriptor();
        resetCallbacks();
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
        mIsBusy = false;
        mPrintingState = PRINTING_STATE_FINISHED;
        closeFileDescriptor();
        // No need to reset callbacks or call onFinish as the system is destroying us.
    }

    @Override
    public boolean hasPrintingFinished() {
        return mPrintingState == PRINTING_STATE_FINISHED;
    }

    @Override
    public int getDpi() {
        return mDpi;
    }

    @Override
    public int getFileDescriptor() {
        return assumeNonNull(mFileDescriptor).getFd();
    }

    @Override
    public int getPageHeight() {
        return assumeNonNull(mMediaSize).getHeightMils();
    }

    @Override
    public int getPageWidth() {
        return assumeNonNull(mMediaSize).getWidthMils();
    }

    @Override
    public int @Nullable [] getPageNumbers() {
        return mPages == null ? null : mPages.clone();
    }

    @Override
    public boolean isBusy() {
        return mIsBusy;
    }

    @VisibleForTesting
    public @Nullable Printable getPrintable() {
        return mPrintable;
    }

    @Override
    public void setPendingPrint(
            final Printable printable,
            PrintManagerDelegate printManager,
            int renderProcessId,
            int renderFrameId) {
        if (mIsBusy) {
            Log.d(TAG, "Pending print can't be set. PrintingController is busy.");
            return;
        }
        mPrintable = printable;
        mErrorMessage = mPrintable.getErrorMessage();
        mPrintManager = printManager;
        mRenderProcessId = renderProcessId;
        mRenderFrameId = renderFrameId;
    }

    @Override
    public void startPendingPrint() {
        boolean canStartPrint = false;
        if (mIsBusy) {
            Log.d(TAG, "Pending print can't be started. PrintingController is busy.");
        } else if (mPrintManager == null) {
            Log.d(TAG, "Pending print can't be started. No PrintManager provided.");
        } else if (mPrintable == null || !mPrintable.canPrint()) {
            Log.d(TAG, "Pending print can't be started. Printable can't perform printing.");
        } else {
            canStartPrint = true;
        }

        if (!canStartPrint) return;

        mIsBusy = true;
        assert mPrintManager != null;
        assert mPrintable != null;
        mPrintDocumentAdapterWrapper.print(mPrintManager, mPrintable.getTitle());
        mPrintManager = null;
    }

    @Override
    public void startPrint(final Printable printable, PrintManagerDelegate printManager) {
        if (mIsBusy) return;
        setPendingPrint(printable, printManager, mRenderProcessId, mRenderFrameId);
        startPendingPrint();
    }

    @Override
    public void pdfWritingDone(int pageCount) {
        if (mPrintingState == PRINTING_STATE_READY) {
            assert pageCount == 0
                    : "There is no pending printing task, should only be a failure report";
        }

        if (mPrintingState != PRINTING_STATE_STARTED_FROM_ONWRITE) return;

        mPrintingState = PRINTING_STATE_READY;
        closeFileDescriptor();
        if (pageCount > 0) {
            PageRange[] pageRanges = convertIntegerArrayToPageRanges(mPages, pageCount);
            assumeNonNull(mOnWriteCallback).onWriteFinished(pageRanges);
        } else {
            assumeNonNull(mOnWriteCallback).onWriteFailed(mErrorMessage);
            resetCallbacks();
        }
    }

    @Override
    public void onStart() {
        mPrintingState = PRINTING_STATE_READY;
    }

    @Override
    public void onLayout(
            PrintAttributes oldAttributes,
            PrintAttributes newAttributes,
            CancellationSignal cancellationSignal,
            PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper callback,
            @Nullable Bundle metadata) {
        // NOTE: Chrome printing just supports one DPI, whereas Android has both vertical and
        // horizontal.  These two values are most of the time same, so we just pass one of them.
        mDpi = assumeNonNull(newAttributes.getResolution()).getHorizontalDpi();
        mMediaSize = newAttributes.getMediaSize();

        mOnLayoutCallback = callback;
        // We don't want to stack Chromium with multiple PDF generation operations before
        // completion of an ongoing one.
        if (mPrintingState == PRINTING_STATE_STARTED_FROM_ONWRITE) {
            callback.onLayoutFailed(mErrorMessage);
            resetCallbacks();
        } else {
            PrintDocumentInfo info =
                    new PrintDocumentInfo.Builder(assumeNonNull(mPrintable).getTitle())
                            .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
                            // Set page count to unknown since Android framework will get it from
                            // PDF file generated in onWrite.
                            .setPageCount(PrintDocumentInfo.PAGE_COUNT_UNKNOWN)
                            .build();
            // We always need to generate a new PDF. onLayout is not only called when attributes
            // changed, but also when pages need to print got selected. We can't tell if the later
            // case was happened, so has to generate a new file.
            mOnLayoutCallback.onLayoutFinished(info, true);
        }
    }

    @Override
    public void onWrite(
            final PageRange[] ranges,
            final ParcelFileDescriptor destination,
            final CancellationSignal cancellationSignal,
            final PrintDocumentAdapterWrapper.WriteResultCallbackWrapper callback) {
        // TODO(cimamoglu): Make use of CancellationSignal.
        if (ranges == null || ranges.length == 0) {
            callback.onWriteFailed(null);
            return;
        }

        mOnWriteCallback = callback;

        assert mPrintingState == PRINTING_STATE_READY;
        assert mFileDescriptor == null;
        try {
            mFileDescriptor = destination.dup();
        } catch (IOException e) {
            mOnWriteCallback.onWriteFailed("ParcelFileDescriptor.dup() failed: " + e.toString());
            resetCallbacks();
            return;
        }
        mPages = convertPageRangesToIntegerArray(ranges);
        String pdfFilePath = assumeNonNull(mPrintable).getPdfFilePath();

        if (pdfFilePath == null) {
            // mRenderProcessId and mRenderFrameId could be invalid values, in this case we are
            // going to
            // print the main frame.
            if (mPrintable.print(mRenderProcessId, mRenderFrameId)) {
                mPrintingState = PRINTING_STATE_STARTED_FROM_ONWRITE;
            } else {
                mOnWriteCallback.onWriteFailed(mErrorMessage);
                resetCallbacks();
            }
            // We are guaranteed by the framework that we will not have two onWrite calls at once.
            // We may get a CancellationSignal, after replying it (via WriteResultCallback) we might
            // get another onWrite call.
        } else {
            // The print job is already a pdf. Copy to destination from the provided filepath.
            onWriteForPdfPage(pdfFilePath, cancellationSignal);
        }
    }

    @Override
    public void onFinish() {
        mPages = null;

        mRenderProcessId = -1;
        mRenderFrameId = -1;

        mPrintingState = PRINTING_STATE_FINISHED;

        closeFileDescriptor();

        resetCallbacks();
        // The printmanager contract is that onFinish() is always called as the last
        // callback. We set busy to false here.
        mIsBusy = false;
    }

    private void onWriteForPdfPage(
            final String pdfFilePath, final CancellationSignal cancellationSignal) {
        mPrintingState = PRINTING_STATE_STARTED_FROM_ONWRITE;
        InputStream inputStream = null;
        OutputStream outputStream = null;
        try {
            File file = new File(pdfFilePath);
            inputStream = new FileInputStream(file);
            outputStream = new FileOutputStream(assumeNonNull(mFileDescriptor).getFileDescriptor());

            int count;
            byte[] data = new byte[BUFFER_SIZE];
            while ((count = inputStream.read(data)) != -1 && !cancellationSignal.isCanceled()) {
                outputStream.write(data, 0, count);
            }

            if (cancellationSignal.isCanceled()) {
                assumeNonNull(mOnWriteCallback).onWriteCancelled();
            } else {
                mPrintingState = PRINTING_STATE_READY;
                closeFileDescriptor();
                assumeNonNull(mOnWriteCallback)
                        .onWriteFinished(new PageRange[] {PageRange.ALL_PAGES});
            }
        } catch (Exception e) {
            assumeNonNull(mOnWriteCallback).onWriteFailed(mErrorMessage);
        } finally {
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
                if (outputStream != null) {
                    outputStream.close();
                }
            } catch (IOException e) {
                Log.w(TAG, "Failed to close input or output stream.");
            }
        }
    }

    private void resetCallbacks() {
        mOnWriteCallback = null;
        mOnLayoutCallback = null;
    }

    private void closeFileDescriptor() {
        if (mFileDescriptor == null) return;
        try {
            mFileDescriptor.close();
        } catch (IOException ioe) {
            /* ignore */
        } finally {
            mFileDescriptor = null;
        }
    }

    private static PageRange[] convertIntegerArrayToPageRanges(
            int @Nullable [] pagesArray, int pageCount) {
        PageRange[] pageRanges;
        if (pagesArray != null) {
            pageRanges = new PageRange[pagesArray.length];
            for (int i = 0; i < pageRanges.length; i++) {
                int page = pagesArray[i];
                pageRanges[i] = new PageRange(page, page);
            }
        } else {
            // null corresponds to all pages in Chromium printing logic.
            pageRanges = new PageRange[] {new PageRange(0, pageCount - 1)};
        }
        return pageRanges;
    }

    /** Gets an array of page ranges and returns an array of integers with all ranges expanded. */
    private static int @Nullable [] convertPageRangesToIntegerArray(final PageRange[] ranges) {
        if (ranges.length == 1 && ranges[0].equals(PageRange.ALL_PAGES)) {
            // null corresponds to all pages in Chromium printing logic.
            return null;
        }

        // Expand ranges into a list of individual numbers.
        ArrayList<Integer> pages = new ArrayList<Integer>();
        for (PageRange range : ranges) {
            for (int i = range.getStart(); i <= range.getEnd(); i++) {
                pages.add(i);
            }
        }

        // Convert the list into array.
        int[] ret = new int[pages.size()];
        Iterator<Integer> iterator = pages.iterator();
        for (int i = 0; i < ret.length; i++) {
            ret[i] = iterator.next().intValue();
        }
        return ret;
    }
}
