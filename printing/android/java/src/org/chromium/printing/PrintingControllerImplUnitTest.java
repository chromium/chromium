// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;

import java.io.InputStream;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link PrintingControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrintingControllerImplUnitTest {
    private static class FakePrintable implements Printable {
        @Override
        public boolean initiatePrint(int renderProcessId, int renderFrameId) {
            return true;
        }

        @Override
        public boolean print(int renderProcessId, int renderFrameId) {
            return true;
        }

        @Override
        public void finishPrint(int renderProcessId, int renderFrameId) {}

        @Override
        public String getTitle() {
            return "Test Title";
        }

        @Override
        public String getErrorMessage() {
            return "Test Error";
        }

        @Override
        public boolean canPrint() {
            return true;
        }

        @Override
        public @Nullable InputStream getPdfInputStream() {
            return null;
        }
    }

    @Test
    public void testStartPendingPrintWhenActivityIsFinishing() {
        ActivityController<Activity> activityController =
                Robolectric.buildActivity(Activity.class).setup();
        Activity activity = activityController.get();
        ActivityWindowAndroid window =
                new ActivityWindowAndroid(
                        activity,
                        /* listenToActivityState= */ false,
                        IntentRequestTracker.createFromActivity(activity),
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ true);

        PrintingControllerImpl printingController =
                (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);

        PrintManagerDelegate failIfCalledPrintManager =
                new PrintManagerDelegate() {
                    @Override
                    public boolean print(
                            String printJobName,
                            PrintDocumentAdapter documentAdapter,
                            @Nullable PrintAttributes attributes) {
                        fail("print() must not be called for a finishing Activity.");
                        return false;
                    }
                };

        AtomicBoolean callbackRan = new AtomicBoolean(false);

        printingController.setPendingPrint(
                new FakePrintable(),
                failIfCalledPrintManager,
                /* renderProcessId= */ -1,
                /* renderFrameId= */ -1);
        printingController.setPendingPrintCallback(() -> callbackRan.set(true));

        activity.finish();
        assertTrue(activity.isFinishing());

        printingController.startPendingPrint();

        // The callback should have been invoked immediately because the Activity is finishing.
        assertTrue(callbackRan.get());
        assertFalse(printingController.isBusy());
        assertTrue(printingController.hasPrintingFinished());

        printingController.onActivityDestroyed();
        window.destroy();
        activityController.destroy();
    }
}
