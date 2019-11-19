// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.shape_detection;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MojoException;
import org.chromium.shape_detection.mojom.BarcodeDetection;
import org.chromium.shape_detection.mojom.BarcodeDetectionProvider;
import org.chromium.shape_detection.mojom.BarcodeDetectorOptions;
import org.chromium.shape_detection.mojom.BarcodeFormat;

/**
 * Service provider to create BarcodeDetection services
 */
public class BarcodeDetectionProviderImpl implements BarcodeDetectionProvider {
    private static final String TAG = "BarcodeProviderImpl";

    public BarcodeDetectionProviderImpl() {}

    @Override
    public void createBarcodeDetection(
            InterfaceRequest<BarcodeDetection> request, BarcodeDetectorOptions options) {
        BarcodeDetection.MANAGER.bind(new BarcodeDetectionImpl(options), request);
    }

    @Override
    public void enumerateSupportedFormats(EnumerateSupportedFormatsResponse callback) {
        // Keep this list in sync with the constants defined in
        // com.google.android.gms.vision.barcode.Barcode and the format hints
        // supported by BarcodeDetectionImpl.
        int[] supportedFormats = {BarcodeFormat.AZTEC, BarcodeFormat.CODE_128,
                BarcodeFormat.CODE_39, BarcodeFormat.CODE_93, BarcodeFormat.CODABAR,
                BarcodeFormat.DATA_MATRIX, BarcodeFormat.EAN_13, BarcodeFormat.EAN_8,
                BarcodeFormat.ITF, BarcodeFormat.PDF417, BarcodeFormat.QR_CODE, BarcodeFormat.UPC_A,
                BarcodeFormat.UPC_E};
        callback.call(supportedFormats);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    public static BarcodeDetectionProvider create() {
        if (GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                    ContextUtils.getApplicationContext())
                != ConnectionResult.SUCCESS) {
            Log.e(TAG, "Google Play Services not available");
            return null;
        }
        return new BarcodeDetectionProviderImpl();
    }
}
