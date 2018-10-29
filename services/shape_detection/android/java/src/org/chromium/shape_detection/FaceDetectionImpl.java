// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.PointF;
import android.media.FaceDetector;
import android.media.FaceDetector.Face;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;
import org.chromium.shape_detection.mojom.Landmark;

/**
 * Android implementation of the FaceDetection service defined in
 * services/shape_detection/public/mojom/facedetection.mojom
 */
public class FaceDetectionImpl implements FaceDetection {
    private static final String TAG = "FaceDetectionImpl";
    private static final int MAX_FACES = 32;
    private final boolean mFastMode;
    private final int mMaxFaces;

    FaceDetectionImpl(FaceDetectorOptions options) {
        mFastMode = options.fastMode;
        mMaxFaces = Math.min(options.maxDetectedFaces, MAX_FACES);
    }

    @Override
    public void detect(org.chromium.skia.mojom.Bitmap bitmapData, final DetectResponse callback) {
        Bitmap bitmap = BitmapUtils.convertToBitmap(bitmapData);
        if (bitmap == null) {
            Log.e(TAG, "Error converting Mojom Bitmap to Android Bitmap");
            callback.call(new FaceDetectionResult[0]);
            return;
        }

        // FaceDetector requires an even width, so pad the image if the width is odd.
        // https://developer.android.com/reference/android/media/FaceDetector.html#FaceDetector(int, int, int)
        final int width = bitmapData.imageInfo.width + (bitmapData.imageInfo.width % 2);
        final int height = bitmapData.imageInfo.height;
        if (width != bitmapData.imageInfo.width) {
            Bitmap paddedBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(paddedBitmap);
            canvas.drawBitmap(bitmap, 0, 0, null);
            bitmap = paddedBitmap;
        }

        // A Bitmap must be in 565 format for findFaces() to work. See
        // http://androidxref.com/7.0.0_r1/xref/frameworks/base/media/java/android/media/FaceDetector.java#124
        //
        // It turns out that FaceDetector is not able to detect correctly if
        // simply using pixmap.setConfig(). The reason might be that findFaces()
        // needs non-premultiplied ARGB arrangement, while the alpha type in the
        // original image is premultiplied. We can use getPixels() which does
        // the unmultiplication while copying to a new array. See
        // http://androidxref.com/7.0.0_r1/xref/frameworks/base/graphics/java/android/graphics/Bitmap.java#538
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        final Bitmap unPremultipliedBitmap =
                Bitmap.createBitmap(pixels, width, height, Bitmap.Config.RGB_565);

        // FaceDetector creation and findFaces() might take a long time and trigger a
        // "StrictMode policy violation": they should happen in a background thread.
        AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                final FaceDetector detector = new FaceDetector(width, height, mMaxFaces);
                Face[] detectedFaces = new Face[mMaxFaces];
                // findFaces() will stop at |mMaxFaces|.
                final int numberOfFaces = detector.findFaces(unPremultipliedBitmap, detectedFaces);

                FaceDetectionResult[] faceArray = new FaceDetectionResult[numberOfFaces];

                for (int i = 0; i < numberOfFaces; i++) {
                    faceArray[i] = new FaceDetectionResult();

                    final Face face = detectedFaces[i];
                    final PointF midPoint = new PointF();
                    face.getMidPoint(midPoint);
                    final float eyesDistance = face.eyesDistance();

                    faceArray[i].boundingBox = new RectF();
                    faceArray[i].boundingBox.x = midPoint.x - eyesDistance;
                    faceArray[i].boundingBox.y = midPoint.y - eyesDistance;
                    faceArray[i].boundingBox.width = 2 * eyesDistance;
                    faceArray[i].boundingBox.height = 2 * eyesDistance;
                    // TODO(xianglu): Consider adding Face.confidence and Face.pose.

                    faceArray[i].landmarks = new Landmark[0];
                }

                callback.call(faceArray);
            }
        });
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }
}
