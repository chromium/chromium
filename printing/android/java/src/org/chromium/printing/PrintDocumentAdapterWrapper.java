// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.printing;

import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Wrapper for {@link PrintDocumentAdapter} for easier testing.
 *
 * Normally, {@link PrintDocumentAdapter#LayoutResultCallback} and
 * {@link PrintDocumentAdapter#WriteResultCallback} don't have public constructors. This makes
 * it impossible to subclass them, which is required to emulate calls to
 * {@link PrintDocumentAdapter#onLayout} and {@link PrintDocumentAdapter#onWrite}. This class helps
 * bypassing the limitation.
 */
@NullMarked
public class PrintDocumentAdapterWrapper extends PrintDocumentAdapter {
    private final PdfGenerator mPdfGenerator;

    public PrintDocumentAdapterWrapper(PdfGenerator pdfGenerator) {
        mPdfGenerator = pdfGenerator;
    }

    public static interface PdfGenerator {
        void onStart();

        void onLayout(
                PrintAttributes oldAttributes,
                PrintAttributes newAttributes,
                CancellationSignal cancellationSignal,
                PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper callback,
                @Nullable Bundle metadata);

        void onWrite(
                final PageRange[] ranges,
                final ParcelFileDescriptor destination,
                final CancellationSignal cancellationSignal,
                final PrintDocumentAdapterWrapper.WriteResultCallbackWrapper callback);

        void onFinish();
    }

    public static interface LayoutResultCallbackWrapper {
        void onLayoutFinished(PrintDocumentInfo info, boolean changed);

        void onLayoutFailed(@Nullable CharSequence error);

        void onLayoutCancelled();
    }

    public static interface WriteResultCallbackWrapper {
        void onWriteFinished(PageRange[] pages);

        void onWriteFailed(@Nullable CharSequence error);

        void onWriteCancelled();
    }

    public static class LayoutResultCallbackWrapperImpl implements LayoutResultCallbackWrapper {
        private final LayoutResultCallback mCallback;

        public LayoutResultCallbackWrapperImpl(LayoutResultCallback callback) {
            assert callback != null;
            mCallback = callback;
        }

        @Override
        public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
            mCallback.onLayoutFinished(info, changed);
        }

        @Override
        public void onLayoutFailed(@Nullable CharSequence error) {
            mCallback.onLayoutFailed(error);
        }

        @Override
        public void onLayoutCancelled() {
            mCallback.onLayoutCancelled();
        }
    }

    public static class WriteResultCallbackWrapperImpl implements WriteResultCallbackWrapper {
        private final WriteResultCallback mCallback;

        public WriteResultCallbackWrapperImpl(WriteResultCallback callback) {
            assert callback != null;
            mCallback = callback;
        }

        @Override
        public void onWriteFinished(PageRange[] pages) {
            mCallback.onWriteFinished(pages);
        }

        @Override
        public void onWriteFailed(@Nullable CharSequence error) {
            mCallback.onWriteFailed(error);
        }

        @Override
        public void onWriteCancelled() {
            mCallback.onWriteCancelled();
        }
    }

    /** Initiates the printing process within the framework */
    public void print(PrintManagerDelegate printManager, String title) {
        printManager.print(title, this, null);
    }

    @Override
    public void onStart() {
        mPdfGenerator.onStart();
    }

    @Override
    public void onLayout(
            PrintAttributes oldAttributes,
            PrintAttributes newAttributes,
            CancellationSignal cancellationSignal,
            LayoutResultCallback callback,
            Bundle metadata) {
        mPdfGenerator.onLayout(
                oldAttributes,
                newAttributes,
                cancellationSignal,
                new LayoutResultCallbackWrapperImpl(callback),
                metadata);
    }

    @Override
    public void onWrite(
            final PageRange[] ranges,
            final ParcelFileDescriptor destination,
            final CancellationSignal cancellationSignal,
            final WriteResultCallback callback) {
        mPdfGenerator.onWrite(
                ranges,
                destination,
                cancellationSignal,
                new WriteResultCallbackWrapperImpl(callback));
    }

    @Override
    public void onFinish() {
        super.onFinish();
        mPdfGenerator.onFinish();
    }
}
