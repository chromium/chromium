// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;

import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;
import com.google.android.gms.vision.face.FaceDetector;
import com.google.android.gms.vision.text.TextRecognizer;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.gms.ChromiumPlayServicesAvailability;
import org.chromium.skia.mojom.BitmapN32ImageInfo;

import java.nio.ByteBuffer;

/**
 * Utility class for ShapeDetection instrumentation tests,
 * provides support for e.g. reading files and converting
 * Bitmaps to mojom.Bitmaps.
 */
public class TestUtils {
    public static final boolean IS_GMS_CORE_SUPPORTED = isGmsCoreSupported();
    public static boolean sIsVisionLibraryReady;

    private static boolean isGmsCoreSupported() {
        return ChromiumPlayServicesAvailability.isGooglePlayServicesAvailable(
                ContextUtils.getApplicationContext());
    }

    public static void waitForVisionLibraryReady() {
        Assert.assertTrue(IS_GMS_CORE_SUPPORTED);
        if (!sIsVisionLibraryReady) {
            FaceDetector.Builder builder =
                    new FaceDetector.Builder(ContextUtils.getApplicationContext());
            builder.setMode(FaceDetector.ACCURATE_MODE);
            builder.setLandmarkType(FaceDetector.ALL_LANDMARKS);
            FaceDetector faceDetector = builder.build();
            CriteriaHelper.pollInstrumentationThread(() -> faceDetector.isOperational());

            BarcodeDetector barcodeDetector =
                    new BarcodeDetector.Builder(ContextUtils.getApplicationContext())
                            .setBarcodeFormats(Barcode.ALL_FORMATS)
                            .build();
            CriteriaHelper.pollInstrumentationThread(() -> barcodeDetector.isOperational());

            TextRecognizer textRecognizer =
                    new TextRecognizer.Builder(ContextUtils.getApplicationContext()).build();
            CriteriaHelper.pollInstrumentationThread(() -> textRecognizer.isOperational());

            sIsVisionLibraryReady = true;
        }
    }

    public static org.chromium.skia.mojom.BitmapN32 mojoBitmapFromBitmap(Bitmap bitmap) {
        ByteBuffer buffer = ByteBuffer.allocate(bitmap.getByteCount());
        bitmap.copyPixelsToBuffer(buffer);

        org.chromium.skia.mojom.BitmapN32 mojoBitmap = new org.chromium.skia.mojom.BitmapN32();
        mojoBitmap.imageInfo = new BitmapN32ImageInfo();
        mojoBitmap.imageInfo.width = bitmap.getWidth();
        mojoBitmap.imageInfo.height = bitmap.getHeight();
        mojoBitmap.pixelData = new org.chromium.mojo_base.mojom.BigBuffer();
        mojoBitmap.pixelData.setBytes(buffer.array());
        return mojoBitmap;
    }

    public static org.chromium.skia.mojom.BitmapN32 mojoBitmapFromFile(String relPath) {
        String path = UrlUtils.getIsolatedTestFilePath("services/test/data/" + relPath);
        Bitmap bitmap = BitmapFactory.decodeFile(path);
        return mojoBitmapFromBitmap(bitmap);
    }

    public static org.chromium.skia.mojom.BitmapN32 mojoBitmapFromText(String[] texts) {
        final int x = 10;
        final int baseline = 100;

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setTextSize(36.0f);
        paint.setTextAlign(Paint.Align.LEFT);

        Bitmap bitmap = Bitmap.createBitmap(1080, 480, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.WHITE);

        for (int i = 0; i < texts.length; i++) {
            canvas.drawText(texts[i], x, baseline * (i + 1), paint);
        }

        return mojoBitmapFromBitmap(bitmap);
    }
}
