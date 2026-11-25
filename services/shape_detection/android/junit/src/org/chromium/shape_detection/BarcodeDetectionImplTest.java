// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.util.SparseArray;

import com.google.android.gms.vision.Detector;
import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.mojo_base.mojom.BigBuffer;
import org.chromium.shape_detection.mojom.BarcodeDetectionResult;
import org.chromium.shape_detection.mojom.BarcodeFormat;
import org.chromium.skia.mojom.BitmapN32;
import org.chromium.skia.mojom.BitmapN32ImageInfo;

/** Unit tests for BarcodeDetectionImpl dimension guards. */
@RunWith(BaseRobolectricTestRunner.class)
public class BarcodeDetectionImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Detector<Barcode> mBarcodeDetector;

    private BarcodeDetectionImpl mDetector;

    @Before
    public void setUp() {
        when(mBarcodeDetector.isOperational()).thenReturn(true);
        mDetector = new BarcodeDetectionImpl(mBarcodeDetector);
    }

    private static BitmapN32 createMojoBitmap(int width, int height) {
        BitmapN32 bitmap = new BitmapN32();
        bitmap.imageInfo = new BitmapN32ImageInfo();
        bitmap.imageInfo.width = width;
        bitmap.imageInfo.height = height;
        bitmap.pixelData = new BigBuffer();
        bitmap.pixelData.setBytes(new byte[width * height * 4]);
        return bitmap;
    }

    private static BitmapN32 createMojoBitmapMetadataOnly(int width, int height) {
        BitmapN32 bitmap = new BitmapN32();
        bitmap.imageInfo = new BitmapN32ImageInfo();
        bitmap.imageInfo.width = width;
        bitmap.imageInfo.height = height;
        bitmap.pixelData = new BigBuffer();
        bitmap.pixelData.setBytes(new byte[0]);
        return bitmap;
    }

    private static Barcode createFakeBarcode(
            String value, int left, int top, int right, int bottom) {
        Barcode barcode = new Barcode();
        barcode.rawValue = value;
        barcode.format = Barcode.QR_CODE;
        barcode.cornerPoints =
                new Point[] {
                    new Point(left, top),
                    new Point(right, top),
                    new Point(right, bottom),
                    new Point(left, bottom),
                };
        return barcode;
    }

    private BarcodeDetectionResult[] detect(BitmapN32 bitmap) {
        final BarcodeDetectionResult[][] captured = new BarcodeDetectionResult[1][];
        mDetector.detect(bitmap, results -> captured[0] = results);
        assertNotNull(captured[0]);
        return captured[0];
    }

    @Test
    @Feature({"ShapeDetection"})
    public void testRejectsTooSmallDimensions() {
        assertEquals(0, detect(createMojoBitmapMetadataOnly(-1, 100)).length);
        assertEquals(0, detect(createMojoBitmapMetadataOnly(100, -1)).length);
        assertEquals(0, detect(createMojoBitmapMetadataOnly(0, 100)).length);
        assertEquals(0, detect(createMojoBitmapMetadataOnly(100, 0)).length);
        int underMin = BarcodeDetectionImpl.MIN_IMAGE_DIMENSION - 1;
        assertEquals(0, detect(createMojoBitmap(1, 100)).length);
        assertEquals(0, detect(createMojoBitmap(100, 1)).length);
        assertEquals(0, detect(createMojoBitmap(underMin, 100)).length);
        assertEquals(0, detect(createMojoBitmap(100, underMin)).length);
        assertEquals(0, detect(createMojoBitmap(underMin, underMin)).length);
        verify(mBarcodeDetector, never()).detect(any());
    }

    @Test
    @Feature({"ShapeDetection"})
    public void testRejectsOversizedDimensions() {
        int overMax = BarcodeDetectionImpl.MAX_IMAGE_DIMENSION + 1;
        assertEquals(0, detect(createMojoBitmap(overMax, 100)).length);
        assertEquals(0, detect(createMojoBitmap(100, overMax)).length);
        assertEquals(0, detect(createMojoBitmap(overMax, overMax)).length);
        verify(mBarcodeDetector, never()).detect(any());
    }

    @Test
    @Feature({"ShapeDetection"})
    public void testAcceptsValidDimensions() {
        SparseArray<Barcode> fakeResults = new SparseArray<>();
        fakeResults.put(0, createFakeBarcode("https://chromium.org", 10, 20, 60, 40));
        when(mBarcodeDetector.detect(any())).thenReturn(fakeResults);

        BarcodeDetectionResult[] results = detect(createMojoBitmap(100, 50));

        ArgumentCaptor<Frame> frameCaptor = ArgumentCaptor.forClass(Frame.class);
        verify(mBarcodeDetector).detect(frameCaptor.capture());
        Bitmap passedBitmap = frameCaptor.getValue().getBitmap();
        assertNotNull(passedBitmap);
        assertEquals(100, passedBitmap.getWidth());
        assertEquals(50, passedBitmap.getHeight());

        assertEquals(1, results.length);
        assertEquals("https://chromium.org", results[0].rawValue);
        assertEquals(BarcodeFormat.QR_CODE, results[0].format);
        assertEquals(10.0f, results[0].boundingBox.x, 0.01f);
        assertEquals(20.0f, results[0].boundingBox.y, 0.01f);
        assertEquals(50.0f, results[0].boundingBox.width, 0.01f);
        assertEquals(20.0f, results[0].boundingBox.height, 0.01f);
    }

    @Test
    @Feature({"ShapeDetection"})
    public void testAcceptsBoundaryDimensions() {
        when(mBarcodeDetector.detect(any())).thenReturn(new SparseArray<>());

        int min = BarcodeDetectionImpl.MIN_IMAGE_DIMENSION;
        int max = BarcodeDetectionImpl.MAX_IMAGE_DIMENSION;
        assertEquals(0, detect(createMojoBitmap(min, min)).length);
        assertEquals(0, detect(createMojoBitmap(max, min)).length);
        assertEquals(0, detect(createMojoBitmap(min, max)).length);
        assertEquals(0, detect(createMojoBitmap(max, max)).length);
        verify(mBarcodeDetector, times(4)).detect(any());
    }
}
