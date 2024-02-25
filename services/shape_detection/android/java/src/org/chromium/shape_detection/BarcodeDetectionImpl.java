// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.SparseArray;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.gfx.mojom.PointF;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionResult;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;
import org.chromium.shape_detection.mojom.BarcodeFormat;

/** Implementation of mojo BarcodeDetection, using Google Play Services vision package. */
public class BarcodeDetectionImpl implements BarcodeDetection {
    private static final String TAG = "BarcodeDetectionImpl";

    private BarcodeDetector mBarcodeDetector;

    public BarcodeDetectionImpl(BarcodeDetectorOptions options) {
        int formats = Barcode.ALL_FORMATS;
        if (options.formats != null && options.formats.length > 0) {
            formats = 0;
            // Keep this list in sync with the constants defined in
            // com.google.android.gms.vision.barcode.Barcode and the list of
            // supported formats in BarcodeDetectionProviderImpl.
            for (int i = 0; i < options.formats.length; ++i) {
                if (options.formats[i] == BarcodeFormat.AZTEC) {
                    formats |= Barcode.AZTEC;
                } else if (options.formats[i] == BarcodeFormat.CODE_128) {
                    formats |= Barcode.CODE_128;
                } else if (options.formats[i] == BarcodeFormat.CODE_39) {
                    formats |= Barcode.CODE_39;
                } else if (options.formats[i] == BarcodeFormat.CODE_93) {
                    formats |= Barcode.CODE_93;
                } else if (options.formats[i] == BarcodeFormat.CODABAR) {
                    formats |= Barcode.CODABAR;
                } else if (options.formats[i] == BarcodeFormat.DATA_MATRIX) {
                    formats |= Barcode.DATA_MATRIX;
                } else if (options.formats[i] == BarcodeFormat.EAN_13) {
                    formats |= Barcode.EAN_13;
                } else if (options.formats[i] == BarcodeFormat.EAN_8) {
                    formats |= Barcode.EAN_8;
                } else if (options.formats[i] == BarcodeFormat.ITF) {
                    formats |= Barcode.ITF;
                } else if (options.formats[i] == BarcodeFormat.PDF417) {
                    formats |= Barcode.PDF417;
                } else if (options.formats[i] == BarcodeFormat.QR_CODE) {
                    formats |= Barcode.QR_CODE;
                } else if (options.formats[i] == BarcodeFormat.UPC_A) {
                    formats |= Barcode.UPC_A;
                } else if (options.formats[i] == BarcodeFormat.UPC_E) {
                    formats |= Barcode.UPC_E;
                } else {
                    Log.e(TAG, "Unsupported barcode format hint: " + options.formats[i]);
                }
            }
        }
        mBarcodeDetector =
                new BarcodeDetector.Builder(ContextUtils.getApplicationContext())
                        .setBarcodeFormats(formats)
                        .build();
    }

    @Override
    public void detect(org.chromium.skia.mojom.BitmapN32 bitmapData, Detect_Response callback) {
        // The vision library will be downloaded the first time the API is used
        // on the device; this happens "fast", but it might have not completed,
        // bail in this case. Also, the API was disabled between and v.9.0 and
        // v.9.2, see https://developers.google.com/android/guides/releases.
        if (!mBarcodeDetector.isOperational()) {
            Log.e(TAG, "BarcodeDetector is not operational");
            callback.call(new BarcodeDetectionResult[0]);
            return;
        }

        Frame frame = BitmapUtils.convertToFrame(bitmapData);
        if (frame == null) {
            Log.e(TAG, "Error converting Mojom Bitmap to Frame");
            callback.call(new BarcodeDetectionResult[0]);
            return;
        }

        final SparseArray<Barcode> barcodes = mBarcodeDetector.detect(frame);

        BarcodeDetectionResult[] barcodeArray = new BarcodeDetectionResult[barcodes.size()];
        for (int i = 0; i < barcodes.size(); i++) {
            barcodeArray[i] = new BarcodeDetectionResult();
            final Barcode barcode = barcodes.valueAt(i);
            barcodeArray[i].rawValue = barcode.rawValue;
            final Rect rect = barcode.getBoundingBox();
            barcodeArray[i].boundingBox = new RectF();
            barcodeArray[i].boundingBox.x = rect.left;
            barcodeArray[i].boundingBox.y = rect.top;
            barcodeArray[i].boundingBox.width = rect.width();
            barcodeArray[i].boundingBox.height = rect.height();
            final Point[] corners = barcode.cornerPoints;
            barcodeArray[i].cornerPoints = new PointF[corners.length];
            for (int j = 0; j < corners.length; j++) {
                barcodeArray[i].cornerPoints[j] = new PointF();
                barcodeArray[i].cornerPoints[j].x = corners[j].x;
                barcodeArray[i].cornerPoints[j].y = corners[j].y;
            }
            barcodeArray[i].format = toBarcodeFormat(barcode.format);
        }
        callback.call(barcodeArray);
    }

    @Override
    public void close() {
        mBarcodeDetector.release();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    private int toBarcodeFormat(int format) {
        switch (format) {
            case Barcode.CODE_128:
                return BarcodeFormat.CODE_128;
            case Barcode.CODE_39:
                return BarcodeFormat.CODE_39;
            case Barcode.CODE_93:
                return BarcodeFormat.CODE_93;
            case Barcode.CODABAR:
                return BarcodeFormat.CODABAR;
            case Barcode.DATA_MATRIX:
                return BarcodeFormat.DATA_MATRIX;
            case Barcode.EAN_13:
                return BarcodeFormat.EAN_13;
            case Barcode.EAN_8:
                return BarcodeFormat.EAN_8;
            case Barcode.ITF:
                return BarcodeFormat.ITF;
            case Barcode.QR_CODE:
                return BarcodeFormat.QR_CODE;
            case Barcode.UPC_A:
                return BarcodeFormat.UPC_A;
            case Barcode.UPC_E:
                return BarcodeFormat.UPC_E;
            case Barcode.PDF417:
                return BarcodeFormat.PDF417;
            case Barcode.AZTEC:
                return BarcodeFormat.AZTEC;
        }
        return BarcodeFormat.UNKNOWN;
    }
}
