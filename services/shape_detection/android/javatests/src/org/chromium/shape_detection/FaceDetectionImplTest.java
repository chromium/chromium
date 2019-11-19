// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.RectF;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for FaceDetectionImpl.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FaceDetectionImplTest {
    private static final org.chromium.skia.mojom.Bitmap MONA_LISA_BITMAP =
            TestUtils.mojoBitmapFromFile("mona_lisa.jpg");
    private static final org.chromium.skia.mojom.Bitmap FACE_POSE_BITMAP =
            TestUtils.mojoBitmapFromFile("face_pose.png");
    // Different versions of Android have different implementations of FaceDetector.findFaces(), so
    // we have to use a large error threshold.
    private static final double BOUNDING_BOX_POSITION_ERROR = 10.0;
    private static final double BOUNDING_BOX_SIZE_ERROR = 5.0;
    private static final float ACCURATE_MODE_SIZE = 2.0f;
    private static enum DetectionProviderType { ANDROID, GMS_CORE }

    public FaceDetectionImplTest() {}

    private static FaceDetectionResult[] detect(org.chromium.skia.mojom.Bitmap mojoBitmap,
            boolean fastMode, DetectionProviderType api) {
        FaceDetectorOptions options = new FaceDetectorOptions();
        options.fastMode = fastMode;
        options.maxDetectedFaces = 32;
        FaceDetection detector = null;
        if (api == DetectionProviderType.ANDROID) {
            detector = new FaceDetectionImpl(options);
        } else if (api == DetectionProviderType.GMS_CORE) {
            detector = new FaceDetectionImplGmsCore(options);
        } else {
            assert false;
            return null;
        }

        final ArrayBlockingQueue<FaceDetectionResult[]> queue = new ArrayBlockingQueue<>(1);
        detector.detect(mojoBitmap, new FaceDetection.DetectResponse() {
            @Override
            public void call(FaceDetectionResult[] results) {
                queue.add(results);
            }
        });
        FaceDetectionResult[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Could not get FaceDetectionResult: " + e.toString());
        }
        Assert.assertNotNull(toReturn);
        return toReturn;
    }

    private void detectSucceedsOnValidImage(DetectionProviderType api) {
        FaceDetectionResult[] results = detect(MONA_LISA_BITMAP, true, api);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(
                api == DetectionProviderType.GMS_CORE ? 4 : 0, results[0].landmarks.length);
        Assert.assertEquals(40.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(40.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(24.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        Assert.assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithAndroidAPI() {
        detectSucceedsOnValidImage(DetectionProviderType.ANDROID);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithGmsCore() {
        if (TestUtils.IS_GMS_CORE_SUPPORTED) {
            detectSucceedsOnValidImage(DetectionProviderType.GMS_CORE);
        }
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectHandlesOddWidthWithAndroidAPI() {
        // Pad the image so that the width is odd.
        Bitmap paddedBitmap = Bitmap.createBitmap(MONA_LISA_BITMAP.imageInfo.width + 1,
                MONA_LISA_BITMAP.imageInfo.height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        canvas.drawBitmap(BitmapUtils.convertToBitmap(MONA_LISA_BITMAP), 0, 0, null);
        org.chromium.skia.mojom.Bitmap mojoBitmap = TestUtils.mojoBitmapFromBitmap(paddedBitmap);
        Assert.assertEquals(1, mojoBitmap.imageInfo.width % 2);

        FaceDetectionResult[] results = detect(mojoBitmap, true, DetectionProviderType.ANDROID);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(40.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(40.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(24.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        Assert.assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectFacesInProfileWithGmsCore() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        FaceDetectionResult[] fastModeResults =
                detect(FACE_POSE_BITMAP, true, DetectionProviderType.GMS_CORE);
        Assert.assertEquals(4, fastModeResults.length);

        FaceDetectionResult[] unorderedResults =
                detect(FACE_POSE_BITMAP, false, DetectionProviderType.GMS_CORE);
        FaceDetectionResult[] accurateModeResults =
                new FaceDetectionResult[unorderedResults.length];
        for (int i = 0; i < accurateModeResults.length; i++) {
            accurateModeResults[i] = new FaceDetectionResult();
        }
        Assert.assertEquals(4, accurateModeResults.length);
        // Order face results align with fast mode's order which is different from accurate mode.
        accurateModeResults[0].boundingBox = unorderedResults[1].boundingBox;
        accurateModeResults[1].boundingBox = unorderedResults[2].boundingBox;
        accurateModeResults[2].boundingBox = unorderedResults[0].boundingBox;
        accurateModeResults[3].boundingBox = unorderedResults[3].boundingBox;

        // The face bounding box of using ACCURATE_MODE is smaller than FAST_MODE
        for (int i = 0; i < accurateModeResults.length; i++) {
            RectF fastModeRect = new RectF();
            RectF accurateModeRect = new RectF();

            fastModeRect.set(fastModeResults[i].boundingBox.x, fastModeResults[i].boundingBox.y,
                    fastModeResults[i].boundingBox.x + fastModeResults[i].boundingBox.width,
                    fastModeResults[i].boundingBox.y + fastModeResults[i].boundingBox.height);

            accurateModeRect.set(accurateModeResults[i].boundingBox.x + ACCURATE_MODE_SIZE,
                    accurateModeResults[i].boundingBox.y + ACCURATE_MODE_SIZE,
                    accurateModeResults[i].boundingBox.x + accurateModeResults[i].boundingBox.width
                            - ACCURATE_MODE_SIZE,
                    accurateModeResults[i].boundingBox.y + accurateModeResults[i].boundingBox.height
                            - ACCURATE_MODE_SIZE);

            Assert.assertEquals(true, fastModeRect.contains(accurateModeRect));
        }
    }

    private void detectRotatedFace(Matrix matrix) {
        // Get the bitmap of fourth face in face_pose.png
        Bitmap fourthFace = Bitmap.createBitmap(
                BitmapUtils.convertToBitmap(FACE_POSE_BITMAP), 508, 0, 182, 194);
        int width = fourthFace.getWidth();
        int height = fourthFace.getHeight();

        Bitmap rotationBitmap = Bitmap.createBitmap(fourthFace, 0, 0, width, height, matrix, true);
        FaceDetectionResult[] results = detect(TestUtils.mojoBitmapFromBitmap(rotationBitmap),
                false, DetectionProviderType.GMS_CORE);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(197.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(246.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
    }

    @Test
    @SmallTest
    @Feature({"ShapeDetection"})
    public void testDetectRotatedFaceWithGmsCore() {
        if (!TestUtils.IS_GMS_CORE_SUPPORTED) {
            return;
        }
        Matrix matrix = new Matrix();

        // Rotate the Bitmap.
        matrix.postRotate(15);
        detectRotatedFace(matrix);

        matrix.reset();
        matrix.postRotate(30);
        detectRotatedFace(matrix);

        matrix.reset();
        matrix.postRotate(40);
        detectRotatedFace(matrix);
    }
}
