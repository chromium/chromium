// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

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

    // Called along window.print() path to initialize a printing job.
    @CalledByNative
    public void showPrintDialog() {
        ThreadUtils.assertOnUiThread();
        if (mController.isBusy()) {
            // Already printing. Resolve the native callback with CANCEL immediately.
            showSystemDialogDone();
            return;
        }
        mController.setPendingPrintCallback(this::showSystemDialogDone);
        mController.startPendingPrint();
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
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) return;

        PrintingController printingController = PrintingControllerImpl.getInstance(window);
        printingController.setPendingPrint(
                printable, new PrintManagerDelegateImpl(activity), renderProcessId, renderFrameId);
    }

    private void showSystemDialogDone() {
        assert mNativeObject != 0;
        PrintingContextJni.get().showSystemDialogDone(mNativeObject);
    }

    @NativeMethods
    interface Natives {
        void showSystemDialogDone(long nativePrintingContextAndroid);
    }
}
