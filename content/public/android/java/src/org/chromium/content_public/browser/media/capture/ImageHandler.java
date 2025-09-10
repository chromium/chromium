// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.media.capture;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.media.Image;
import android.media.Image.Plane;
import android.media.ImageReader;
import android.os.Handler;
import android.view.Surface;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

/**
 * ImageHandler manages the lifetime of Images from an ImageReader. It communicates new Images and
 * lifetime related events back to ScreenCapture.
 */
@NullMarked
class ImageHandler implements ImageReader.OnImageAvailableListener {
    interface Delegate {
        void onRgbaFrameAvailable(
                ImageHandler imageHandler,
                Runnable releaseCb,
                long timestampNs,
                Plane plane,
                Rect cropRect);

        void onI420FrameAvailable(
                ImageHandler imageHandler,
                Runnable releaseCb,
                long timestampNs,
                Plane[] planes,
                Rect cropRect);

        void onClose(ImageHandler imageHandler);

        void recreateImageHandler(ScreenCapture.CaptureState captureState);
    }

    private final ScreenCapture.CaptureState mCaptureState;
    private final Delegate mDelegate;
    private final Handler mHandler;
    private final ImageReader mImageReader;
    private int mAcquiredImageCount;
    private boolean mClosing;

    /**
     * Constructs an ImageHandler.
     *
     * @param captureState The state describing the capture (e.g. width, height).
     * @param delegate The delegate to notify of capture events.
     * @param handler The handler on which to run callbacks.
     */
    ImageHandler(ScreenCapture.CaptureState captureState, Delegate delegate, Handler handler) {
        this(
                captureState,
                delegate,
                handler,
                ImageReader.newInstance(
                        captureState.width,
                        captureState.height,
                        captureState.format,
                        /* maxImages= */ 2));
    }

    @VisibleForTesting
    ImageHandler(
            ScreenCapture.CaptureState captureState,
            Delegate delegate,
            Handler handler,
            ImageReader imageReader) {
        mCaptureState = captureState;
        mDelegate = delegate;
        mHandler = handler;
        mImageReader = imageReader;
        mImageReader.setOnImageAvailableListener(this, mHandler);
    }

    Surface getSurface() {
        return mImageReader.getSurface();
    }

    ScreenCapture.CaptureState getCaptureState() {
        return mCaptureState;
    }

    /**
     * Marks the ImageHandler to be closed when all acquired images are released.
     *
     * <p>If there are no acquired images this will close the underlying {@link ImageReader}
     * immediately.
     */
    void close() {
        // If we have no acquired images, then it's safe to close the ImageReader because
        // we are guaranteed that the native side is not using any of those Images.
        if (mAcquiredImageCount == 0) {
            closeNow();
        } else {
            mClosing = true;
        }
    }

    /**
     * Closes the ImageHandler and underlying {@link ImageReader} immediately.
     *
     * <p>This method should only be called when it is certain that no more images will be accessed,
     * including buffers of those images on the C++ side. It is undefined behaviour (e.g. memory
     * safety issues) to access data from an `Image` after it is closed.
     */
    void closeNow() {
        mImageReader.close();
        mAcquiredImageCount = 0;
        mDelegate.onClose(this);
    }

    private @Nullable Image maybeAcquireImage(ImageReader reader) {
        assert reader == mImageReader;
        // If we have acquired the maximum number of images `acquireLatestImage`
        // will print warning level logspam, so avoid
        if (mAcquiredImageCount >= reader.getMaxImages()) return null;

        try {
            final Image image = reader.acquireLatestImage();
            if (image != null) mAcquiredImageCount++;
            return image;
        } catch (IllegalStateException ex) {
            // This happens if we have acquired the maximum number of images without closing
            // them. We will eventually close the images so this is not an error condition.
        } catch (UnsupportedOperationException ex) {
            // This can happen if the `PixelFormat` does not match. We should recreate the
            // `ImageReader` in this case. But there is no way to know what format the producer
            // is using, so we just need to try a bunch of common ones.
            final int recreateFormat =
                    switch (mCaptureState.format) {
                        case PixelFormat.RGBA_8888 -> ImageFormat.YUV_420_888;
                        default -> throw new IllegalStateException(
                                "No fallback format remaining from: " + mCaptureState.format);
                    };

            mDelegate.recreateImageHandler(
                    new ScreenCapture.CaptureState(
                            mCaptureState.width,
                            mCaptureState.height,
                            mCaptureState.dpi,
                            recreateFormat));
        }

        return null;
    }

    private void releaseImage(ImageReader reader, Image image) {
        assert reader == mImageReader;

        // If we recreate the ImageReader, we may get an old release here. The image will
        // already have been closed since the ImageReader is closed, but it's safe to call
        // close again here.
        image.close();
        mAcquiredImageCount--;

        if (mClosing) {
            if (mAcquiredImageCount == 0) closeNow();
        } else {
            // Now that we closed an image, we may be able to acquire a new image.
            onImageAvailable(reader);
        }
    }

    @Override
    public void onImageAvailable(ImageReader reader) {
        // Note that we can't use `acquireLatestImage` here because we can't close older
        // images until the C++ side is finished using them.
        final Image image = maybeAcquireImage(reader);

        // If we have not yet closed images, this may return null. We need to retry
        // after closing an image.
        if (image == null) return;

        switch (image.getFormat()) {
            case PixelFormat.RGBA_8888:
                assert image.getPlanes().length == 1;
                final Plane plane = image.getPlanes()[0];
                mDelegate.onRgbaFrameAvailable(
                        this,
                        () -> releaseImage(reader, image),
                        image.getTimestamp(),
                        plane,
                        image.getCropRect());
                break;
            case ImageFormat.YUV_420_888:
                assert image.getPlanes().length == 3;
                mDelegate.onI420FrameAvailable(
                        this,
                        () -> releaseImage(reader, image),
                        image.getTimestamp(),
                        image.getPlanes(),
                        image.getCropRect());
                break;
            default:
                throw new IllegalStateException("Unexpected image format: " + image.getFormat());
        }
    }

    /** Returns the current number of images acquired but not yet released. */
    int getAcquiredImageCountForTesting() {
        return mAcquiredImageCount;
    }

    /** Returns true if close() has been called but images are still pending release. */
    boolean isClosingForTesting() {
        return mClosing;
    }
}
