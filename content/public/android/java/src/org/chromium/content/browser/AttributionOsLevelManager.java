// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.view.MotionEvent;

import androidx.privacysandbox.ads.adservices.java.measurement.MeasurementManagerFutures;
import androidx.privacysandbox.ads.adservices.measurement.DeletionRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceParams;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceRegistrationRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerParams;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerRegistrationRequest;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.url.GURL;

import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Handles passing registrations with Web Attribution Reporting API to the underlying native
 * library.
 */
@JNINamespace("content")
public class AttributionOsLevelManager {
    private static final String TAG = "AttributionManager";
    // TODO: replace with constant in android.Manifest.permission once it becomes available in U.
    private static final String PERMISSION_ACCESS_ADSERVICES_ATTRIBUTION =
            "android.permission.ACCESS_ADSERVICES_ATTRIBUTION";
    private long mNativePtr;
    private MeasurementManagerFutures mManager;

    @CalledByNative
    private AttributionOsLevelManager(long nativePtr) {
        mNativePtr = nativePtr;
    }

    private MeasurementManagerFutures getManager() {
        if (mManager != null) return mManager;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return null;
        }
        mManager = MeasurementManagerFutures.from(ContextUtils.getApplicationContext());
        return mManager;
    }

    private void onRegistrationCompleted(int requestId, boolean success) {
        if (mNativePtr != 0) {
            AttributionOsLevelManagerJni.get().onRegistrationCompleted(
                    mNativePtr, requestId, success);
        }
    }

    private void addRegistrationFutureCallback(int requestId, ListenableFuture<?> future) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }
        Futures.addCallback(future, new FutureCallback<Object>() {
            @Override
            public void onSuccess(Object result) {
                onRegistrationCompleted(requestId, /*success=*/true);
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "Failed to register", thrown);
                onRegistrationCompleted(requestId, /*success=*/false);
            }
        }, ContextUtils.getApplicationContext().getMainExecutor());
    }

    /**
     * Registers a web attribution source with native, see `registerWebSourceAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void registerAttributionSource(int requestId, GURL registrationUrl, GURL topLevelOrigin,
            boolean isDebugKeyAllowed, MotionEvent event) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onRegistrationCompleted(requestId, /*success=*/false);
            return;
        }
        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onRegistrationCompleted(requestId, /*success=*/false);
            return;
        }
        ListenableFuture<?> future = mm.registerWebSourceAsync(new WebSourceRegistrationRequest(
                Arrays.asList(new WebSourceParams(
                        Uri.parse(registrationUrl.getSpec()), isDebugKeyAllowed)),
                Uri.parse(topLevelOrigin.getSpec()), /*inputEvent=*/event,
                /*appDestination=*/null, /*webDestination=*/null,
                /*verifiedDestination=*/null));
        addRegistrationFutureCallback(requestId, future);
    }

    /**
     * Registers a web attribution trigger with native, see `registerWebTriggerAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void registerAttributionTrigger(
            int requestId, GURL registrationUrl, GURL topLevelOrigin, boolean isDebugKeyAllowed) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onRegistrationCompleted(requestId, /*success=*/false);
            return;
        }

        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onRegistrationCompleted(requestId, /*success=*/false);
            return;
        }
        ListenableFuture<?> future = mm.registerWebTriggerAsync(new WebTriggerRegistrationRequest(
                Arrays.asList(new WebTriggerParams(
                        Uri.parse(registrationUrl.getSpec()), isDebugKeyAllowed)),
                Uri.parse(topLevelOrigin.getSpec())));
        addRegistrationFutureCallback(requestId, future);
    }

    private void onDataDeletionCompleted(int requestId) {
        if (mNativePtr != 0) {
            AttributionOsLevelManagerJni.get().onDataDeletionCompleted(mNativePtr, requestId);
        }
    }

    /**
     * Deletes attribution data with native, see `deleteRegistrationsAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void deleteRegistrations(int requestId, long startMs, long endMs, GURL[] origins,
            String[] domains, int deletionMode, int matchBehavior) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onDataDeletionCompleted(requestId);
            return;
        }
        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onDataDeletionCompleted(requestId);
            return;
        }
        ArrayList<Uri> originUris = new ArrayList<Uri>(origins.length);
        for (GURL origin : origins) {
            originUris.add(Uri.parse(origin.getSpec()));
        }

        ArrayList<Uri> domainUris = new ArrayList<Uri>(domains.length);
        for (String domain : domains) {
            domainUris.add(Uri.parse(domain));
        }

        ListenableFuture<?> future = mm.deleteRegistrationsAsync(
                new DeletionRequest(deletionMode, matchBehavior, Instant.ofEpochMilli(startMs),
                        Instant.ofEpochMilli(endMs), originUris, domainUris));

        Futures.addCallback(future, new FutureCallback<Object>() {
            @Override
            public void onSuccess(Object result) {
                onDataDeletionCompleted(requestId);
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "Failed to delete measurement API data", thrown);
                onDataDeletionCompleted(requestId);
            }
        }, ContextUtils.getApplicationContext().getMainExecutor());
    }

    /**
     * Gets Measurement API status with native, see `getMeasurementApiStatusAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void getMeasurementApiStatus() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
            return;
        }
        if (ContextUtils.getApplicationContext().checkPermission(
                    PERMISSION_ACCESS_ADSERVICES_ATTRIBUTION, Process.myPid(), Process.myUid())
                != PackageManager.PERMISSION_GRANTED) {
            // Permission may not be granted when embedded as WebView.
            AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
            return;
        }
        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
            return;
        }

        ListenableFuture<Integer> future = null;
        try {
            future = mm.getMeasurementApiStatusAsync();
        } catch (IllegalStateException ex) {
            // An illegal state exception may be thrown for some versions of the underlying
            // Privacy Sandbox SDK.
            Log.i(TAG, "Failed to get measurement API status", ex);
        }

        if (future == null) {
            AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
            return;
        }

        Futures.addCallback(future, new FutureCallback<Integer>() {
            @Override
            public void onSuccess(Integer status) {
                AttributionOsLevelManagerJni.get().onMeasurementStateReturned(status);
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "Failed to get measurement API status", thrown);
                AttributionOsLevelManagerJni.get().onMeasurementStateReturned(0);
            }
        }, ContextUtils.getApplicationContext().getMainExecutor());
    }

    @CalledByNative
    private void nativeDestroyed() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void onDataDeletionCompleted(long nativeAttributionOsLevelManagerAndroid, int requestId);
        void onRegistrationCompleted(
                long nativeAttributionOsLevelManagerAndroid, int requestId, boolean success);
        void onMeasurementStateReturned(int state);
    }
}
