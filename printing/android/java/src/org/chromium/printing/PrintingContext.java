// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class is responsible for communicating with its native counterpart through JNI to handle
 * the generation of PDF.  On the Java side, it works with a {@link PrintingController}
 * to talk to the framework.
 */
@JNINamespace("printing")
public class PrintingContext {
    private static final String TAG = "Printing";

    /** The controller this object interacts with, which in turn communicates with the framework. */
    private final PrintingController mController;

    /** The pointer to the native PrintingContextAndroid object. */
    private final long mNativeObject;

    private PrintingContext(long ptr) {
        mController = PrintingControllerImpl.getInstance();
        mNativeObject = ptr;
    }

    @CalledByNative
    public static PrintingContext create(long nativeObjectPointer) {
        ThreadUtils.assertOnUiThread();
        return new PrintingContext(nativeObjectPointer);
    }

    @CalledByNative
    public int getFileDescriptor() {
        ThreadUtils.assertOnUiThread();
        return mController.getFileDescriptor();
    }

    @CalledByNative
    public int getDpi() {
        ThreadUtils.assertOnUiThread();
        return mController.getDpi();
    }

    @CalledByNative
    public int getWidth() {
        ThreadUtils.assertOnUiThread();
        return mController.getPageWidth();
    }

    @CalledByNative
    public int getHeight() {
        ThreadUtils.assertOnUiThread();
        return mController.getPageHeight();
    }

    // Called along window.print() path to initialize a printing job.
    @CalledByNative
    public void showPrintDialog() {
        ThreadUtils.assertOnUiThread();
        if (mController != null) { // The native side doesn't check if printing is enabled
            mController.startPendingPrint();
        }
        // Reply to native side with |CANCEL| since there is no printing settings available yet at
        // this stage.
        showSystemDialogDone();
    }

    @CalledByNative
    public static void pdfWritingDone(int pageCount) {
        ThreadUtils.assertOnUiThread();
        if (PrintingControllerImpl.getInstance() != null) {
            PrintingControllerImpl.getInstance().pdfWritingDone(pageCount);
        } else {
            Log.d(TAG, "No PrintingController, can't notify print completion.");
        }
    }

    @CalledByNative
    public int[] getPages() {
        ThreadUtils.assertOnUiThread();
        return mController.getPageNumbers();
    }

    @CalledByNative
    public void askUserForSettings(final int maxPages) {
        ThreadUtils.assertOnUiThread();
        // If the printing dialog has already finished, tell Chromium that operation is cancelled.
        if (mController.hasPrintingFinished()) {
            // NOTE: We don't call nativeAskUserForSettingsReply (hence Chromium callback in
            // AskUserForSettings callback) twice.
            askUserForSettingsReply(false);
        } else {
            mController.setPrintingContext(this);
            askUserForSettingsReply(true);
        }
    }

    private void askUserForSettingsReply(boolean success) {
        assert mNativeObject != 0;
        nativeAskUserForSettingsReply(mNativeObject, success);
    }

    private void showSystemDialogDone() {
        assert mNativeObject != 0;
        nativeShowSystemDialogDone(mNativeObject);
    }

    private native void nativeAskUserForSettingsReply(
            long nativePrintingContextAndroid, boolean success);

    private native void nativeShowSystemDialogDone(long nativePrintingContextAndroid);
}
