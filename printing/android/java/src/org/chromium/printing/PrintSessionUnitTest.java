// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import static org.junit.Assert.assertEquals;

import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.Nullable;
import org.chromium.printing.PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper;
import org.chromium.printing.PrintDocumentAdapterWrapper.WriteResultCallbackWrapper;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

/** Unit tests for {@link PrintSession}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PrintSessionUnitTest {
    private static final PrintAttributes ATTRIBUTES =
            new PrintAttributes.Builder()
                    .setMediaSize(PrintAttributes.MediaSize.ISO_A4)
                    .setResolution(new PrintAttributes.Resolution("id", "label", 300, 300))
                    .setMinMargins(PrintAttributes.Margins.NO_MARGINS)
                    .build();

    private static class FakePdfPrintable implements Printable {
        private final InputStream mPdfInputStream;

        FakePdfPrintable(InputStream pdfInputStream) {
            mPdfInputStream = pdfInputStream;
        }

        @Override
        public boolean initiatePrint(int renderProcessId, int renderFrameId) {
            return true;
        }

        @Override
        public boolean print(int renderProcessId, int renderFrameId) {
            return false;
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
            return mPdfInputStream;
        }
    }

    private static class RecordingWriteCallback implements WriteResultCallbackWrapper {
        int mFinishedCount;
        int mFailedCount;
        int mCancelledCount;

        @Override
        public void onWriteFinished(PageRange[] pages) {
            mFinishedCount++;
        }

        @Override
        public void onWriteFailed(@Nullable CharSequence error) {
            mFailedCount++;
        }

        @Override
        public void onWriteCancelled() {
            mCancelledCount++;
        }
    }

    private static class RecordingLayoutCallback implements LayoutResultCallbackWrapper {
        int mFinishedCount;
        int mFailedCount;

        @Override
        public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
            mFinishedCount++;
        }

        @Override
        public void onLayoutFailed(@Nullable CharSequence error) {
            mFailedCount++;
        }

        @Override
        public void onLayoutCancelled() {}
    }

    @Rule public TemporaryFolder mTempFolder = new TemporaryFolder();

    private PrintManagerDelegate mPrintManager;

    @Before
    public void setUp() {
        mPrintManager =
                new PrintManagerDelegate() {
                    @Override
                    public boolean print(
                            String printJobName,
                            PrintDocumentAdapter documentAdapter,
                            @Nullable PrintAttributes attributes) {
                        return true;
                    }
                };
    }

    private ParcelFileDescriptor openDestination() throws IOException {
        return ParcelFileDescriptor.open(
                mTempFolder.newFile(),
                ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_TRUNCATE);
    }

    private void assertLayoutSucceeds(PrintSession session) {
        RecordingLayoutCallback layoutCallback = new RecordingLayoutCallback();
        session.onLayout(ATTRIBUTES, ATTRIBUTES, new CancellationSignal(), layoutCallback, null);
        assertEquals(1, layoutCallback.mFinishedCount);
        assertEquals(0, layoutCallback.mFailedCount);
    }

    @Test
    public void testLayoutSucceedsAfterCancelledPdfWrite() throws IOException {
        PrintSession session =
                new PrintSession(
                        new FakePdfPrintable(new ByteArrayInputStream(new byte[] {1, 2, 3})),
                        mPrintManager,
                        /* renderProcessId= */ -1,
                        /* renderFrameId= */ -1);
        session.onStart();
        assertLayoutSucceeds(session);

        CancellationSignal cancellationSignal = new CancellationSignal();
        RecordingWriteCallback writeCallback = new RecordingWriteCallback();
        session.onWrite(
                new PageRange[] {PageRange.ALL_PAGES},
                openDestination(),
                cancellationSignal,
                writeCallback);
        cancellationSignal.cancel();
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(1, writeCallback.mCancelledCount);
        assertLayoutSucceeds(session);

        // The write was already answered, so teardown must not answer it again.
        session.destroy();
        assertEquals(0, writeCallback.mFailedCount);
        assertEquals(0, writeCallback.mFinishedCount);
    }

    @Test
    public void testLayoutSucceedsAfterFailedPdfWrite() throws IOException {
        InputStream failingStream =
                new InputStream() {
                    @Override
                    public int read() throws IOException {
                        throw new IOException("Simulated read failure");
                    }
                };
        PrintSession session =
                new PrintSession(
                        new FakePdfPrintable(failingStream),
                        mPrintManager,
                        /* renderProcessId= */ -1,
                        /* renderFrameId= */ -1);
        session.onStart();
        assertLayoutSucceeds(session);

        RecordingWriteCallback writeCallback = new RecordingWriteCallback();
        session.onWrite(
                new PageRange[] {PageRange.ALL_PAGES},
                openDestination(),
                new CancellationSignal(),
                writeCallback);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(1, writeCallback.mFailedCount);
        assertLayoutSucceeds(session);

        session.destroy();
        assertEquals(1, writeCallback.mFailedCount);
        assertEquals(0, writeCallback.mFinishedCount);
        assertEquals(0, writeCallback.mCancelledCount);
    }
}
