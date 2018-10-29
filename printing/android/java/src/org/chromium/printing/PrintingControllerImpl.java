// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentInfo;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.printing.PrintDocumentAdapterWrapper.PdfGenerator;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Controls the interactions with Android framework related to printing.
 *
 * This class is singleton, since at any point at most one printing dialog can exist. Also, since
 * this dialog is modal, user can't interact with the browser unless they close the dialog or press
 * the print button. The singleton object lives in UI thread. Interaction with the native side is
 * carried through PrintingContext class.
 */
@TargetApi(Build.VERSION_CODES.KITKAT)
public class PrintingControllerImpl implements PrintingController, PdfGenerator {
    private static final String TAG = "printing";

    /**
     * This is used for both initial state and a completed state (i.e. starting from either
     * onLayout or onWrite, a PDF generation cycle is completed another new one can safely start).
     */
    private static final int PRINTING_STATE_READY = 0;
    private static final int PRINTING_STATE_STARTED_FROM_ONWRITE = 1;
    /** Printing dialog has been dismissed and cleanup has been done. */
    private static final int PRINTING_STATE_FINISHED = 2;

    /** The singleton instance for this class. */
    @VisibleForTesting
    protected static PrintingController sInstance;

    private final String mErrorMessage;

    private PrintingContext mPrintingContext;

    private int mRenderProcessId;
    private int mRenderFrameId;

    /** The file descriptor into which the PDF will be written.  Provided by the framework. */
    private ParcelFileDescriptor mFileDescriptor;

    /** Dots per inch, as provided by the framework. */
    private int mDpi;

    /** Paper dimensions. */
    private PrintAttributes.MediaSize mMediaSize;

    /** Numbers of pages to be printed, zero indexed. */
    private int[] mPages;

    /** The callback function to inform the result of PDF generation to the framework. */
    private PrintDocumentAdapterWrapper.WriteResultCallbackWrapper mOnWriteCallback;

    /**
     * The callback function to inform the result of layout to the framework.  We save the callback
     * because we start the native PDF generation process inside onLayout, and we need to pass the
     * number of expected pages back to the framework through this callback once the native side
     * has that information.
     */
    private PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper mOnLayoutCallback;

    /** The object through which native PDF generation process is initiated. */
    private Printable mPrintable;

    /** The object through which the framework will make calls for generating PDF. */
    private PrintDocumentAdapterWrapper mPrintDocumentAdapterWrapper;

    private int mPrintingState = PRINTING_STATE_READY;

    private boolean mIsBusy;

    private PrintManagerDelegate mPrintManager;

    @VisibleForTesting
    protected PrintingControllerImpl(
            PrintDocumentAdapterWrapper printDocumentAdapterWrapper, String errorText) {
        mErrorMessage = errorText;
        mPrintDocumentAdapterWrapper = printDocumentAdapterWrapper;
        mPrintDocumentAdapterWrapper.setPdfGenerator(this);
    }

    /**
     * Creates a controller for handling printing with the framework.
     *
     * The controller is a singleton, since there can be only one printing action at any time.
     *
     * @param printDocumentAdapterWrapper The object through which the framework will make calls
     *                                    for generating PDF.
     * @param errorText The error message to be shown to user in case something goes wrong in PDF
     *                  generation in Chromium. We pass it here as a string so src/printing/android
     *                  doesn't need any string dependency.
     * @return The resulting PrintingController.
     */
    public static PrintingController create(
            PrintDocumentAdapterWrapper printDocumentAdapterWrapper, String errorText) {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            sInstance = new PrintingControllerImpl(printDocumentAdapterWrapper, errorText);
        }
        return sInstance;
    }

    /**
     * Returns the singleton instance, created by the {@link PrintingControllerImpl#create}.
     *
     * This method must be called once {@link PrintingControllerImpl#create} is called, and always
     * thereafter.
     *
     * @return The singleton instance.
     */
    public static PrintingController getInstance() {
        return sInstance;
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
        return mFileDescriptor.getFd();
    }

    @Override
    public int getPageHeight() {
        return mMediaSize.getHeightMils();
    }

    @Override
    public int getPageWidth() {
        return mMediaSize.getWidthMils();
    }

    @Override
    public int[] getPageNumbers() {
        return mPages == null ? null : mPages.clone();
    }

    @Override
    public boolean isBusy() {
        return mIsBusy;
    }

    @Override
    public void setPrintingContext(final PrintingContext printingContext) {
        mPrintingContext = printingContext;
    }

    @Override
    public void setPendingPrint(final Printable printable, PrintManagerDelegate printManager,
            int renderProcessId, int renderFrameId) {
        if (mIsBusy) {
            Log.d(TAG, "Pending print can't be set. PrintingController is busy.");
            return;
        }
        mPrintable = printable;
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
        } else if (!mPrintable.canPrint()) {
            Log.d(TAG, "Pending print can't be started. Printable can't perform printing.");
        } else {
            canStartPrint = true;
        }

        if (!canStartPrint) return;

        mIsBusy = true;
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
        if (mPrintingState == PRINTING_STATE_FINISHED) return;
        mPrintingState = PRINTING_STATE_READY;
        closeFileDescriptor();
        if (pageCount > 0) {
            PageRange[] pageRanges = convertIntegerArrayToPageRanges(mPages, pageCount);
            mOnWriteCallback.onWriteFinished(pageRanges);
        } else {
            mOnWriteCallback.onWriteFailed(mErrorMessage);
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
            Bundle metadata) {
        // NOTE: Chrome printing just supports one DPI, whereas Android has both vertical and
        // horizontal.  These two values are most of the time same, so we just pass one of them.
        mDpi = newAttributes.getResolution().getHorizontalDpi();
        mMediaSize = newAttributes.getMediaSize();

        mOnLayoutCallback = callback;
        // We don't want to stack Chromium with multiple PDF generation operations before
        // completion of an ongoing one.
        if (mPrintingState == PRINTING_STATE_STARTED_FROM_ONWRITE) {
            callback.onLayoutFailed(mErrorMessage);
            resetCallbacks();
        } else {
            PrintDocumentInfo info =
                    new PrintDocumentInfo.Builder(mPrintable.getTitle())
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

        // mRenderProcessId and mRenderFrameId could be invalid values, in this case we are going to
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
    }

    @Override
    public void onFinish() {
        mPages = null;
        mPrintingContext = null;

        mRenderProcessId = -1;
        mRenderFrameId = -1;

        mPrintingState = PRINTING_STATE_FINISHED;

        closeFileDescriptor();

        resetCallbacks();
        // The printmanager contract is that onFinish() is always called as the last
        // callback. We set busy to false here.
        mIsBusy = false;
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

    private static PageRange[] convertIntegerArrayToPageRanges(int[] pagesArray, int pageCount) {
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

    /**
     * Gets an array of page ranges and returns an array of integers with all ranges expanded.
     */
    private static int[] convertPageRangesToIntegerArray(final PageRange[] ranges) {
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
