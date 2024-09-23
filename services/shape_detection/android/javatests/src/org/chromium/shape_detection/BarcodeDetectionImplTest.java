// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Manual;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.BarcodeDetectionResult;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;
import org.chromium.shape_detection.mojom.BarcodeFormat;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Test suite for BarcodeDetectionImpl. */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.UNIT_TESTS)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class BarcodeDetectionImplTest {
    private static final float BOUNDS_TOLERANCE = 4.0f;

    private static final org.chromium.skia.mojom.BitmapN32 QR_CODE_BITMAP =
            TestUtils.mojoBitmapFromFile("qr_code.png");

    private static final int[] SUPPORTED_FORMATS = {
        BarcodeFormat.AZTEC,
        BarcodeFormat.CODE_128,
        BarcodeFormat.CODE_39,
        BarcodeFormat.CODE_93,
        BarcodeFormat.CODABAR,
        BarcodeFormat.DATA_MATRIX,
        BarcodeFormat.EAN_13,
        BarcodeFormat.EAN_8,
        BarcodeFormat.ITF,
        BarcodeFormat.PDF417,
        BarcodeFormat.QR_CODE,
        BarcodeFormat.UPC_A,
        BarcodeFormat.UPC_E
    };

    private static int[] enumerateSupportedFormats() {
        BarcodeDetectionProvider provider = new BarcodeDetectionProviderImpl();

        final ArrayBlockingQueue<int[]> queue = new ArrayBlockingQueue<>(13);
        provider.enumerateSupportedFormats(
                new BarcodeDetectionProvider.EnumerateSupportedFormats_Response() {
                    @Override
                    public void call(int[] results) {
                        queue.add(results);
                    }
                });
        int[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Could not get int[] supported formats: " + e.toString());
        }
        Assert.assertNotNull(toReturn);
        return toReturn;
    }

    private static BarcodeDetectionResult[] detect(org.chromium.skia.mojom.BitmapN32 mojoBitmap) {
        BarcodeDetectorOptions options = new BarcodeDetectorOptions();
        return detectWithOptions(mojoBitmap, options);
    }

    private static BarcodeDetectionResult[] detectWithHint(
            org.chromium.skia.mojom.BitmapN32 mojoBitmap, int format) {
        Assert.assertTrue(BarcodeFormat.isKnownValue(format));
        BarcodeDetectorOptions options = new BarcodeDetectorOptions();
        options.formats = new int[] {format};
        return detectWithOptions(mojoBitmap, options);
    }

    private static BarcodeDetectionResult[] detectWithOptions(
            org.chromium.skia.mojom.BitmapN32 mojoBitmap, BarcodeDetectorOptions options) {
        BarcodeDetection detector = new BarcodeDetectionImpl(options);

        final ArrayBlockingQueue<BarcodeDetectionResult[]> queue = new ArrayBlockingQueue<>(1);
        detector.detect(
                mojoBitmap,
                new BarcodeDetection.Detect_Response() {
                    @Override
                    public void call(BarcodeDetectionResult[] results) {
                        queue.add(results);
                    }
                });
        BarcodeDetectionResult[] toReturn = null;
        try {
            toReturn = queue.poll(5L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Could not get BarcodeDetectionResult: " + e.toString());
        }
        Assert.assertNotNull(toReturn);
        return toReturn;
    }

    @Before
    public void setUp() {
        TestUtils.waitForVisionLibraryReady();
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testEnumerateSupportedFormats() {
        int[] results = enumerateSupportedFormats();
        Assert.assertArrayEquals(SUPPORTED_FORMATS, results);
    }

    public static class BarcodeExampleParams implements ParameterProvider {
        private static List<ParameterSet> sBarcodeExampleParams =
                Arrays.asList(
                        // TODO(crbug.com/40159200): AZTEC format is failing.
                        // new ParameterSet()
                        //         .value("aztec.png", BarcodeFormat.AZTEC, "Chromium", 11, 11, 61,
                        // 61)
                        //         .name("AZTEC"),
                        new ParameterSet()
                                .value(
                                        "codabar.png",
                                        BarcodeFormat.CODABAR,
                                        "A6.2831853B",
                                        24,
                                        24,
                                        448,
                                        95)
                                .name("CODABAR"),
                        new ParameterSet()
                                .value(
                                        "code_39.png",
                                        BarcodeFormat.CODE_39,
                                        "CHROMIUM",
                                        20,
                                        20,
                                        318,
                                        75)
                                .name("CODE_39"),
                        new ParameterSet()
                                .value(
                                        "code_93.png",
                                        BarcodeFormat.CODE_93,
                                        "CHROMIUM",
                                        20,
                                        20,
                                        216,
                                        75)
                                .name("CODE_93"),
                        new ParameterSet()
                                .value(
                                        "code_128.png",
                                        BarcodeFormat.CODE_128,
                                        "Chromium",
                                        20,
                                        20,
                                        246,
                                        75)
                                .name("CODE_128"),
                        new ParameterSet()
                                .value(
                                        "data_matrix.png",
                                        BarcodeFormat.DATA_MATRIX,
                                        "Chromium",
                                        11,
                                        11,
                                        53,
                                        53)
                                .name("DATA_MATIX"),
                        new ParameterSet()
                                .value(
                                        "ean_8.png",
                                        BarcodeFormat.EAN_8,
                                        "62831857",
                                        14,
                                        10,
                                        132,
                                        75)
                                .name("EAN_8"),
                        new ParameterSet()
                                .value(
                                        "ean_13.png",
                                        BarcodeFormat.EAN_13,
                                        "6283185307179",
                                        27,
                                        10,
                                        188,
                                        75)
                                .name("EAN_13"),
                        new ParameterSet()
                                .value(
                                        "itf.png",
                                        BarcodeFormat.ITF,
                                        "62831853071795",
                                        10,
                                        10,
                                        135,
                                        39)
                                .name("ITF"),
                        new ParameterSet()
                                .value(
                                        "pdf417.png",
                                        BarcodeFormat.PDF417,
                                        "Chromium",
                                        20,
                                        20,
                                        240,
                                        44)
                                .name("PDF417"),
                        new ParameterSet()
                                .value(
                                        "qr_code.png",
                                        BarcodeFormat.QR_CODE,
                                        "https://chromium.org",
                                        40,
                                        40,
                                        250,
                                        250)
                                .name("QR_CODE"),
                        new ParameterSet()
                                .value(
                                        "upc_a.png",
                                        BarcodeFormat.UPC_A,
                                        "628318530714",
                                        23,
                                        10,
                                        188,
                                        75)
                                .name("UPC_A"),
                        new ParameterSet()
                                .value(
                                        "upc_e.png",
                                        BarcodeFormat.UPC_E,
                                        "06283186",
                                        23,
                                        10,
                                        100,
                                        75)
                                .name("UPC_E"));

        @Override
        public List<ParameterSet> getParameters() {
            return sBarcodeExampleParams;
        }
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @UseMethodParameter(BarcodeExampleParams.class)
    @Feature({"ShapeDetection"})
    public void testDetectBarcodeWithHint(
            String inputFile,
            int format,
            String value,
            float x,
            float y,
            float width,
            float height) {
        org.chromium.skia.mojom.BitmapN32 bitmap = TestUtils.mojoBitmapFromFile(inputFile);
        BarcodeDetectionResult[] results = detectWithHint(bitmap, format);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(value, results[0].rawValue);
        Assert.assertEquals(x, results[0].boundingBox.x, BOUNDS_TOLERANCE);
        Assert.assertEquals(y, results[0].boundingBox.y, BOUNDS_TOLERANCE);
        Assert.assertEquals(width, results[0].boundingBox.width, BOUNDS_TOLERANCE);
        Assert.assertEquals(height, results[0].boundingBox.height, BOUNDS_TOLERANCE);
        Assert.assertEquals(format, results[0].format);
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @UseMethodParameter(BarcodeExampleParams.class)
    @Feature({"ShapeDetection"})
    public void testDetectBarcodeWithoutHint(
            String inputFile,
            int format,
            String value,
            float x,
            float y,
            float width,
            float height) {
        org.chromium.skia.mojom.BitmapN32 bitmap = TestUtils.mojoBitmapFromFile(inputFile);
        BarcodeDetectionResult[] results = detect(bitmap);
        Assert.assertEquals(1, results.length);
        Assert.assertEquals(value, results[0].rawValue);
        Assert.assertEquals(x, results[0].boundingBox.x, BOUNDS_TOLERANCE);
        Assert.assertEquals(y, results[0].boundingBox.y, BOUNDS_TOLERANCE);
        Assert.assertEquals(width, results[0].boundingBox.width, BOUNDS_TOLERANCE);
        Assert.assertEquals(height, results[0].boundingBox.height, BOUNDS_TOLERANCE);
        Assert.assertEquals(format, results[0].format);
    }

    @Test
    @Manual(message = "https://crbug.com/40159200. Require multiple GMSCore libraries.")
    @Feature({"ShapeDetection"})
    public void testTryDetectQrCodeWithAztecHint() {
        BarcodeDetectionResult[] results = detectWithHint(QR_CODE_BITMAP, BarcodeFormat.AZTEC);
        Assert.assertEquals(0, results.length);
    }
}
