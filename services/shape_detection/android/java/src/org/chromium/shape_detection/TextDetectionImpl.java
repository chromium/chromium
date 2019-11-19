// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.SparseArray;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.text.TextBlock;
import com.google.android.gms.vision.text.TextRecognizer;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.gfx.mojom.PointF;
import org.chromium.gfx.mojom.RectF;
import org.chromium.mojo.system.MojoException;
import org.chromium.shape_detection.mojom.TextDetection;
import org.chromium.shape_detection.mojom.TextDetectionResult;


/**
 * Implementation of mojo TextDetection, using Google Play Services vision package.
 */
public class TextDetectionImpl implements TextDetection {
    private static final String TAG = "TextDetectionImpl";

    private TextRecognizer mTextRecognizer;

    public TextDetectionImpl() {
        mTextRecognizer = new TextRecognizer.Builder(ContextUtils.getApplicationContext()).build();
    }

    @Override
    public void detect(org.chromium.skia.mojom.Bitmap bitmapData, DetectResponse callback) {
        // The vision library will be downloaded the first time the API is used
        // on the device; this happens "fast", but it might have not completed,
        // bail in this case. Also, the API was disabled between and v.9.0 and
        // v.9.2, see https://developers.google.com/android/guides/releases.
        if (!mTextRecognizer.isOperational()) {
            Log.e(TAG, "TextDetector is not operational");
            callback.call(new TextDetectionResult[0]);
            return;
        }

        Frame frame = BitmapUtils.convertToFrame(bitmapData);
        if (frame == null) {
            Log.e(TAG, "Error converting Mojom Bitmap to Frame");
            callback.call(new TextDetectionResult[0]);
            return;
        }

        final SparseArray<TextBlock> textBlocks = mTextRecognizer.detect(frame);

        TextDetectionResult[] detectedTextArray = new TextDetectionResult[textBlocks.size()];
        for (int i = 0; i < textBlocks.size(); i++) {
            detectedTextArray[i] = new TextDetectionResult();
            final TextBlock textBlock = textBlocks.valueAt(i);
            detectedTextArray[i].rawValue = textBlock.getValue();
            final Rect rect = textBlock.getBoundingBox();
            detectedTextArray[i].boundingBox = new RectF();
            detectedTextArray[i].boundingBox.x = rect.left;
            detectedTextArray[i].boundingBox.y = rect.top;
            detectedTextArray[i].boundingBox.width = rect.width();
            detectedTextArray[i].boundingBox.height = rect.height();
            final Point[] corners = textBlock.getCornerPoints();
            detectedTextArray[i].cornerPoints = new PointF[corners.length];
            for (int j = 0; j < corners.length; j++) {
                detectedTextArray[i].cornerPoints[j] = new PointF();
                detectedTextArray[i].cornerPoints[j].x = corners[j].x;
                detectedTextArray[i].cornerPoints[j].y = corners[j].y;
            }
        }
        callback.call(detectedTextArray);
    }

    @Override
    public void close() {
        mTextRecognizer.release();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    public static TextDetection create() {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                    ContextUtils.getApplicationContext())
                != ConnectionResult.SUCCESS) {
            Log.e(TAG, "Google Play Services not available");
            return null;
        }
        return new TextDetectionImpl();
    }
}
