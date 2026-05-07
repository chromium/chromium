// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import static org.chromium.printing.PrintingControllerImpl.INVALID_FD;

import android.app.Activity;
import android.os.ParcelFileDescriptor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

import java.io.IOException;

/**
 * This class is responsible for communicating with its native counterpart through JNI to handle the
 * generation of PDF. On the Java side, it works with a {@link PrintingController} to talk to the
 * framework.
 */
@JNINamespace("printing")
@NullMarked
public class PrintingContext {
    private static final String TAG = "Printing";

    /** The controller this object interacts with, which in turn communicates with the framework. */
    private final PrintingController mController;

    /** The pointer to the native PrintingContextAndroid object. */
    private final long mNativeObject;

    private PrintingContext(long ptr, WindowAndroid window) {
        mController = PrintingControllerImpl.getInstance(window);
        mNativeObject = ptr;
    }

    @CalledByNative
    public static PrintingContext create(long nativeObjectPointer, WindowAndroid window) {
        ThreadUtils.assertOnUiThread();
        return new PrintingContext(nativeObjectPointer, window);
    }

    /**
     * Takes a duplicated file descriptor stored in the controller. The caller (typically native
     * code) takes ownership of the returned file descriptor and is responsible for closing it. This
     * is done to prevent Use-After-Close issues by ensuring both Java and C++ have their own
     * independent references to the file.
     *
     * @return The duplicated file descriptor, or {@link PrintingControllerImpl#INVALID_FD} if
     *     failed.
     */
    @CalledByNative
    public int takeDuplicatedFileDescriptor() {
        ThreadUtils.assertOnUiThread();
        ParcelFileDescriptor pfd = mController.getParcelFileDescriptor();
        if (pfd == null) return INVALID_FD;
        try {
            // Duplicate the file descriptor to pass ownership to C++.
            // This prevents UAC as C++ holds its own reference.
            return pfd.dup().detachFd();
        } catch (IOException e) {
            Log.e(TAG, "Failed to duplicate file descriptor", e);
            return INVALID_FD;
        }
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
    public static void pdfWritingDone(int pageCount, WindowAndroid window) {
        ThreadUtils.assertOnUiThread();

        PrintingControllerImpl.getInstance(window).pdfWritingDone(pageCount);
    }

    @CalledByNative
    private static void setPendingPrint(
            WindowAndroid window, Printable printable, int renderProcessId, int renderFrameId) {
        Activity activity = window.getActivity().get();
        if (activity == null) return;

        PrintingController printingController = PrintingControllerImpl.getInstance(window);
        printingController.setPendingPrint(
                printable, new PrintManagerDelegateImpl(activity), renderProcessId, renderFrameId);
    }

    @CalledByNative
    public int @Nullable [] getPages() {
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
        PrintingContextJni.get().askUserForSettingsReply(mNativeObject, success);
    }

    private void showSystemDialogDone() {
        assert mNativeObject != 0;
        PrintingContextJni.get().showSystemDialogDone(mNativeObject);
    }

    @NativeMethods
    interface Natives {
        void askUserForSettingsReply(long nativePrintingContextAndroid, boolean success);

        void showSystemDialogDone(long nativePrintingContextAndroid);
    }
}
