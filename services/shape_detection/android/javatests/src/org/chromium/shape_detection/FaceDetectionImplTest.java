// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import static org.hamcrest.Matchers.greaterThan;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.RectF;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Manual;
import org.chromium.shape_detection.mojom.FaceDetection;
import org.chromium.shape_detection.mojom.FaceDetectionResult;
import org.chromium.shape_detection.mojom.FaceDetectorOptions;

import java.util.Arrays;
import java.util.Comparator;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Test suite for FaceDetectionImpl. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FaceDetectionImplTest {
    private static final org.chromium.skia.mojom.BitmapN32 MONA_LISA_BITMAP =
            TestUtils.mojoBitmapFromFile("mona_lisa.jpg");
    private static final org.chromium.skia.mojom.BitmapN32 FACE_POSE_BITMAP =
            TestUtils.mojoBitmapFromFile("face_pose.png");
    // Different versions of Android have different implementations of FaceDetector.findFaces(), so
    // we have to use a large error threshold.
    private static final double BOUNDING_BOX_POSITION_ERROR = 10.0;
    private static final double BOUNDING_BOX_SIZE_ERROR = 25.0;
    private static final float ACCURATE_MODE_SIZE = 2.0f;
    private static final PointF[] POSE_FACES_LOCATIONS = {
        new PointF(94.0f, 118.0f),
        new PointF(240.0f, 118.0f),
        new PointF(410.0f, 118.0f),
        new PointF(597.0f, 118.0f)
    };

    private static enum DetectionProviderType {
        ANDROID,
        GMS_CORE
    }

    public FaceDetectionImplTest() {}

    private static FaceDetectionResult[] detect(
            org.chromium.skia.mojom.BitmapN32 mojoBitmap,
            boolean fastMode,
            DetectionProviderType api) {
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
        detector.detect(
                mojoBitmap,
                new FaceDetection.Detect_Response() {
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
        Assert.assertEquals(36.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(38.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(26.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        Assert.assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }

    @Before
    public void setUp() {
        TestUtils.waitForVisionLibraryReady();
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithAndroidAPI() {
        detectSucceedsOnValidImage(DetectionProviderType.ANDROID);
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testDetectValidImageWithGmsCore() {
        detectSucceedsOnValidImage(DetectionProviderType.GMS_CORE);
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testDetectHandlesOddWidthWithAndroidAPI() {
        // Pad the image so that the width is odd.
        Bitmap paddedBitmap =
                Bitmap.createBitmap(
                        MONA_LISA_BITMAP.imageInfo.width + 1,
                        MONA_LISA_BITMAP.imageInfo.height,
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(paddedBitmap);
        canvas.drawBitmap(BitmapUtils.convertToBitmap(MONA_LISA_BITMAP), 0, 0, null);
        org.chromium.skia.mojom.BitmapN32 mojoBitmap = TestUtils.mojoBitmapFromBitmap(paddedBitmap);
        Assert.assertEquals(1, mojoBitmap.imageInfo.width % 2);

        FaceDetectionResult[] results = detect(mojoBitmap, true, DetectionProviderType.ANDROID);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(36.0, results[0].boundingBox.width, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(38.0, results[0].boundingBox.height, BOUNDING_BOX_SIZE_ERROR);
        Assert.assertEquals(26.0, results[0].boundingBox.x, BOUNDING_BOX_POSITION_ERROR);
        Assert.assertEquals(20.0, results[0].boundingBox.y, BOUNDING_BOX_POSITION_ERROR);
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testDetectFacesInProfileWithGmsCore() {
        final int numFaces = 4;

        // The 4 faces are in a row.
        Comparator<FaceDetectionResult> sorter =
                (f1, f2) -> {
                    return Float.compare(f1.boundingBox.x, f2.boundingBox.x);
                };

        FaceDetectionResult[] fastModeResults =
                detect(FACE_POSE_BITMAP, true, DetectionProviderType.GMS_CORE);
        Assert.assertEquals(numFaces, fastModeResults.length);
        Arrays.sort(fastModeResults, sorter);

        FaceDetectionResult[] accurateModeResults =
                detect(FACE_POSE_BITMAP, false, DetectionProviderType.GMS_CORE);
        Assert.assertEquals(numFaces, accurateModeResults.length);
        Arrays.sort(accurateModeResults, sorter);

        for (int i = 0; i < numFaces; i++) {
            RectF fastModeRect = new RectF();
            RectF accurateModeRect = new RectF();

            fastModeRect.set(
                    fastModeResults[i].boundingBox.x,
                    fastModeResults[i].boundingBox.y,
                    fastModeResults[i].boundingBox.x + fastModeResults[i].boundingBox.width,
                    fastModeResults[i].boundingBox.y + fastModeResults[i].boundingBox.height);

            accurateModeRect.set(
                    accurateModeResults[i].boundingBox.x,
                    accurateModeResults[i].boundingBox.y,
                    accurateModeResults[i].boundingBox.x + accurateModeResults[i].boundingBox.width,
                    accurateModeResults[i].boundingBox.y
                            + accurateModeResults[i].boundingBox.height);

            Assert.assertTrue(
                    fastModeRect.contains(POSE_FACES_LOCATIONS[i].x, POSE_FACES_LOCATIONS[i].y));
            Assert.assertTrue(
                    accurateModeRect.contains(
                            POSE_FACES_LOCATIONS[i].x, POSE_FACES_LOCATIONS[i].y));
            for (int j = 0; j < numFaces; j++) {
                if (i == j) continue;
                Assert.assertFalse(
                        fastModeRect.contains(
                                POSE_FACES_LOCATIONS[j].x, POSE_FACES_LOCATIONS[j].y));
                Assert.assertFalse(
                        accurateModeRect.contains(
                                POSE_FACES_LOCATIONS[j].x, POSE_FACES_LOCATIONS[j].y));
            }
        }
    }

    private void detectRotatedFace(Matrix matrix) {
        // Get the bitmap of fourth face in face_pose.png
        Bitmap fourthFace =
                Bitmap.createBitmap(
                        BitmapUtils.convertToBitmap(FACE_POSE_BITMAP), 508, 0, 182, 194);
        int width = fourthFace.getWidth();
        int height = fourthFace.getHeight();

        Bitmap rotationBitmap = Bitmap.createBitmap(fourthFace, 0, 0, width, height, matrix, true);
        FaceDetectionResult[] results =
                detect(
                        TestUtils.mojoBitmapFromBitmap(rotationBitmap),
                        false,
                        DetectionProviderType.GMS_CORE);
        Assert.assertEquals(1, results.length);
        Assert.assertThat(results[0].boundingBox.width, greaterThan(100.0f));
        Assert.assertThat(results[0].boundingBox.height, greaterThan(200.0f));
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testDetectRotatedFaceWithGmsCore() {
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
