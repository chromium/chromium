// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.url.GURL;

import java.util.List;

/** Java counterpart of native ImageDownloadCallback. */
public interface ImageDownloadCallback {
    /**
     * Called when image downloading is completed.
     * @param id The unique id for the download image request, which corresponds to the return value
     *                 of {@link WebContents.DownloadImage}.
     * @param httpStatusCode The HTTP status code for the download request.
     * @param imageUrl The URL of the downloaded image.
     * @param bitmaps The bitmaps from the download image. Note that the bitmaps in the image could
     *                 be ignored or resized if they are larger than the size limit in {@link
     *                 WebContente.DownloadImage}.
     * @param originalImageSizes The original sizes of {@link bitmaps} prior to the resizing.
     */
    void onFinishDownloadImage(
            int id,
            int httpStatusCode,
            GURL imageUrl,
            List<Bitmap> bitmaps,
            List<Rect> originalImageSizes);
}
