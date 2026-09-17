// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintAttributes.MediaSize;
import android.print.PrintDocumentInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.printing.PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper;
import org.chromium.printing.PrintDocumentAdapterWrapper.PdfGenerator;
import org.chromium.printing.PrintDocumentAdapterWrapper.WriteResultCallbackWrapper;
import org.chromium.ui.base.WindowAndroid;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Encapsulates the execution state and lifecycle callbacks of a single print job.
 *
 * <p>A {@link PrintSession} is created per print operation and is ephemeral. It owns all mutable
 * job parameters (target frame, attributes, callbacks, and file descriptors). Upon completion or
 * cancellation, {@link #destroy()} cleanly tears down the session, releases references to avoid
 * memory leaks, and closes all open OS resources.
 */
@NullMarked
class PrintSession implements PdfGenerator {
    private static final String TAG = "printing";

    private static final int PRINTING_STATE_READY = 0;
    private static final int PRINTING_STATE_STARTED_FROM_ONWRITE = 1;
    private static final int PRINTING_STATE_FINISHED = 2;

    private static final int BUFFER_SIZE = 8 * 1024; // 8 KB

    private final @Nullable String mErrorMessage;

    private int mRenderProcessId = -1;
    private int mRenderFrameId = -1;

    /** The file descriptor into which the PDF will be written. Provided by the framework. */
    private @Nullable ParcelFileDescriptor mFileDescriptor;

    /** Dots per inch, as provided by the framework. */
    private int mDpi;

    /** Paper dimensions. */
    private @Nullable MediaSize mMediaSize;

    /** Numbers of pages to be printed, zero indexed. */
    private int @Nullable [] mPages;

    /** The callback function to inform the result of PDF generation to the framework. */
    private @Nullable WriteResultCallbackWrapper mOnWriteCallback;

    /** The object through which native PDF generation process is initiated. */
    private @Nullable Printable mPrintable;

    private int mPrintingState = PRINTING_STATE_READY;

    private @Nullable Runnable mPendingPrintCallback;

    private boolean mPrintInitiated;
    private boolean mIsBusy;
    private boolean mDestroyed;

    private @Nullable PrintManagerDelegate mPrintManager;
    private final PrintDocumentAdapterWrapper mAdapterWrapper;

    PrintSession(
            Printable printable,
            PrintManagerDelegate printManager,
            int renderProcessId,
            int renderFrameId) {
        mPrintable = printable;
        mErrorMessage = printable.getErrorMessage();
        mPrintManager = printManager;
        mRenderProcessId = renderProcessId;
        mRenderFrameId = renderFrameId;
        mAdapterWrapper = new PrintDocumentAdapterWrapper(this);
    }

    /**
     * Attempts to start the print job with the system {@link PrintManagerDelegate}.
     *
     * @param window The window hosting the print session.
     * @return true if printing was successfully started, false otherwise.
     */
    boolean start(WindowAndroid window) {
        if (mDestroyed) return false;
        if (mPrintManager == null) {
            Log.d(TAG, "Pending print can't be started. No PrintManager provided.");
            return false;
        }
        if (mPrintable == null || !mPrintable.canPrint()) {
            Log.d(TAG, "Pending print can't be started. Printable can't perform printing.");
            return false;
        }
        // A null Activity is allowed here: WindowAndroid only holds the Activity weakly, so
        // the reference may already be cleared, and some embedders use a WindowAndroid that
        // is not backed by an Activity. PrintManagerDelegateImpl re-validates the Activity
        // state right before calling PrintManager#print().
        Activity activity = window.getActivity().get();
        if (activity != null && (activity.isFinishing() || activity.isDestroyed())) {
            Log.d(TAG, "Pending print can't be started. Activity is finishing or destroyed.");
            return false;
        }

        mIsBusy = true;
        PrintManagerDelegate printManager = mPrintManager;
        mPrintManager = null;
        boolean printStarted = mAdapterWrapper.print(printManager, mPrintable.getTitle());
        if (!printStarted) {
            mIsBusy = false;
            return false;
        }
        return true;
    }

    /**
     * Idempotently destroys the print session, tearing down active renderer operations, closing
     * file descriptors, and clearing all retained references.
     */
    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;

        if (mPrintInitiated && mPrintable != null) {
            mPrintable.finishPrint(mRenderProcessId, mRenderFrameId);
        }
        mPrintInitiated = false;
        mRenderProcessId = -1;
        mRenderFrameId = -1;
        mIsBusy = false;

        closeFileDescriptor();
        // The framework contract is that every onWrite() is answered exactly once. Tearing the
        // session down from the Chromium side (Activity destroyed, window detached) can happen
        // while a write is outstanding, and leaving the callback unanswered wedges the print
        // spooler.
        if (mPrintingState == PRINTING_STATE_STARTED_FROM_ONWRITE && mOnWriteCallback != null) {
            mOnWriteCallback.onWriteFailed(mErrorMessage);
        }
        mPrintingState = PRINTING_STATE_FINISHED;
        resetCallbacks();

        Runnable callback = mPendingPrintCallback;
        mPendingPrintCallback = null;
        mPrintable = null;
        mPrintManager = null;
        mMediaSize = null;
        mPages = null;

        // Run last: this hands control back to native, which may re-enter this object.
        if (callback != null) {
            callback.run();
        }
    }

    boolean isBusy() {
        return mIsBusy;
    }

    boolean hasPrintingFinished() {
        return mPrintingState == PRINTING_STATE_FINISHED;
    }

    int getDpi() {
        return mDpi;
    }

    @Nullable ParcelFileDescriptor getFileDescriptor() {
        return mFileDescriptor;
    }

    int getPageHeight() {
        return mMediaSize == null ? 0 : mMediaSize.getHeightMils();
    }

    int getPageWidth() {
        return mMediaSize == null ? 0 : mMediaSize.getWidthMils();
    }

    int @Nullable [] getPageNumbers() {
        return mPages == null ? null : mPages.clone();
    }

    @VisibleForTesting
    @Nullable Printable getPrintable() {
        return mPrintable;
    }

    void setPendingPrintCallback(@Nullable Runnable callback) {
        mPendingPrintCallback = callback;
    }

    private void initiatePrintIfNeeded() {
        if (!mPrintInitiated
                && mPrintable != null
                && mPrintable.canPrint()
                && mPrintable.getPdfInputStream() == null) {
            mPrintInitiated = mPrintable.initiatePrint(mRenderProcessId, mRenderFrameId);
        }
    }

    @Override
    public void onStart() {
        if (mDestroyed) return;
        mPrintingState = PRINTING_STATE_READY;
        initiatePrintIfNeeded();
    }

    @Override
    public void onLayout(
            PrintAttributes oldAttributes,
            PrintAttributes newAttributes,
            CancellationSignal cancellationSignal,
            LayoutResultCallbackWrapper callback,
            @Nullable Bundle metadata) {
        if (mDestroyed) {
            callback.onLayoutFailed(mErrorMessage);
            return;
        }

        // NOTE: Chrome printing just supports one DPI, whereas Android has both vertical and
        // horizontal. These two values are most of the time same, so we just pass one of them.
        mDpi = assumeNonNull(newAttributes.getResolution()).getHorizontalDpi();
        mMediaSize = newAttributes.getMediaSize();

        // We don't want to stack Chromium with multiple PDF generation operations before
        // completion of an ongoing one.
        if (mPrintingState == PRINTING_STATE_STARTED_FROM_ONWRITE) {
            callback.onLayoutFailed(mErrorMessage);
        } else {
            String title = assumeNonNull(mPrintable).getTitle();
            PrintDocumentInfo info =
                    new PrintDocumentInfo.Builder(title)
                            .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
                            // Set page count to unknown since Android framework will get it from
                            // PDF file generated in onWrite.
                            .setPageCount(PrintDocumentInfo.PAGE_COUNT_UNKNOWN)
                            .build();
            // We always need to generate a new PDF. onLayout is not only called when attributes
            // changed, but also when pages need to print got selected. We can't tell if the later
            // case was happened, so has to generate a new file.
            callback.onLayoutFinished(info, true);
        }
    }

    @Override
    public void onWrite(
            final PageRange[] ranges,
            final ParcelFileDescriptor destination,
            final CancellationSignal cancellationSignal,
            final WriteResultCallbackWrapper callback) {
        if (mDestroyed) {
            callback.onWriteFailed(mErrorMessage);
            try {
                destination.close();
            } catch (IOException e) {
                /* ignore */
            }
            return;
        }

        // TODO(cimamoglu): Make use of CancellationSignal.
        if (ranges == null || ranges.length == 0) {
            callback.onWriteFailed(null);
            try {
                destination.close();
            } catch (IOException e) {
                /* ignore */
            }
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
        } finally {
            try {
                destination.close();
            } catch (IOException e) {
                /* ignore */
            }
        }
        mPages = convertPageRangesToIntegerArray(ranges);
        InputStream pdfInputStream = (mPrintable != null) ? mPrintable.getPdfInputStream() : null;

        if (pdfInputStream == null) {
            if (mPrintable != null && mPrintable.print(mRenderProcessId, mRenderFrameId)) {
                mPrintingState = PRINTING_STATE_STARTED_FROM_ONWRITE;
            } else {
                closeFileDescriptor();
                mOnWriteCallback.onWriteFailed(mErrorMessage);
                resetCallbacks();
            }
        } else {
            // The print job is already a pdf. Copy to destination from the provided InputStream.
            onWriteForPdfPage(pdfInputStream, cancellationSignal);
        }
    }

    @Override
    public void onFinish() {
        // onFinish() can arrive while a write is still outstanding
        // (crbug.com/41401371), but by then the framework has stopped listening
        // for the result, so drop the callback instead of letting destroy()
        // answer it.
        resetCallbacks();
        destroy();
    }

    void pdfWritingDone(int pageCount) {
        if (mDestroyed) return;

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
            resetCallbacks();
        } else {
            assumeNonNull(mOnWriteCallback).onWriteFailed(mErrorMessage);
            resetCallbacks();
        }
    }

    private void onWriteForPdfPage(
            final InputStream inputStream, final CancellationSignal cancellationSignal) {
        mPrintingState = PRINTING_STATE_STARTED_FROM_ONWRITE;
        final ParcelFileDescriptor pfd = mFileDescriptor;
        if (pfd == null) {
            assumeNonNull(mOnWriteCallback).onWriteFailed(mErrorMessage);
            resetCallbacks();
            return;
        }

        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> {
                    OutputStream outputStream = null;
                    boolean success = false;
                    boolean canceled = false;
                    try {
                        outputStream = new FileOutputStream(pfd.getFileDescriptor());

                        int count;
                        byte[] data = new byte[BUFFER_SIZE];
                        while ((count = inputStream.read(data)) != -1
                                && !cancellationSignal.isCanceled()) {
                            outputStream.write(data, 0, count);
                        }
                        outputStream.flush();
                        canceled = cancellationSignal.isCanceled();
                        success = !canceled;
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to write PDF for printing", e);
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

                    final boolean finalSuccess = success;
                    final boolean finalCanceled = canceled;
                    ThreadUtils.postOnUiThread(
                            () -> {
                                if (mDestroyed) return;
                                if (mPrintingState != PRINTING_STATE_STARTED_FROM_ONWRITE) return;
                                closeFileDescriptor();
                                if (finalCanceled) {
                                    assumeNonNull(mOnWriteCallback).onWriteCancelled();
                                } else if (finalSuccess) {
                                    mPrintingState = PRINTING_STATE_READY;
                                    assumeNonNull(mOnWriteCallback)
                                            .onWriteFinished(new PageRange[] {PageRange.ALL_PAGES});
                                } else {
                                    assumeNonNull(mOnWriteCallback).onWriteFailed(mErrorMessage);
                                    resetCallbacks();
                                }
                            });
                });
    }

    private void resetCallbacks() {
        mOnWriteCallback = null;
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

    private static int @Nullable [] convertPageRangesToIntegerArray(final PageRange[] ranges) {
        if (ranges.length == 1 && ranges[0].equals(PageRange.ALL_PAGES)) {
            // null corresponds to all pages in Chromium printing logic.
            return null;
        }

        ArrayList<Integer> pages = new ArrayList<Integer>();
        for (PageRange range : ranges) {
            for (int i = range.getStart(); i <= range.getEnd(); i++) {
                pages.add(i);
            }
        }

        int[] ret = new int[pages.size()];
        Iterator<Integer> iterator = pages.iterator();
        for (int i = 0; i < ret.length; i++) {
            ret[i] = iterator.next().intValue();
        }
        return ret;
    }
}
