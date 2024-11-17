// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.base.WindowAndroid;

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
        mController.startPendingPrint();
        // Reply to native side with |CANCEL| since there is no printing settings available yet at
        // this stage.
        showSystemDialogDone();
    }

    @CalledByNative
    public static void pdfWritingDone(int pageCount) {
        ThreadUtils.assertOnUiThread();

        PrintingControllerImpl.getInstance().pdfWritingDone(pageCount);
    }

    @CalledByNative
    private static void setPendingPrint(
            WindowAndroid window, Printable printable, int renderProcessId, int renderFrameId) {
        Activity activity = window.getActivity().get();
        if (activity == null) return;

        PrintingController printingController = PrintingControllerImpl.getInstance();
        printingController.setPendingPrint(
                printable, new PrintManagerDelegateImpl(activity), renderProcessId, renderFrameId);
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
        // NOTE: We don't call PrintingContextJni.get().askUserForSettingsReply (hence Chromium
        // callback in AskUserForSettings callback) twice.
        askUserForSettingsReply(!mController.hasPrintingFinished());
    }

    private void askUserForSettingsReply(boolean success) {
        assert mNativeObject != 0;
        PrintingContextJni.get()
                .askUserForSettingsReply(mNativeObject, PrintingContext.this, success);
    }

    private void showSystemDialogDone() {
        assert mNativeObject != 0;
        PrintingContextJni.get().showSystemDialogDone(mNativeObject, PrintingContext.this);
    }

    @NativeMethods
    interface Natives {
        void askUserForSettingsReply(
                long nativePrintingContextAndroid, PrintingContext caller, boolean success);

        void showSystemDialogDone(long nativePrintingContextAndroid, PrintingContext caller);
    }
}
