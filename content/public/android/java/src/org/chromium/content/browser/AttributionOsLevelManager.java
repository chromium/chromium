// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.LimitExceededException;
import android.os.Process;
import android.view.MotionEvent;

import androidx.annotation.IntDef;
import androidx.privacysandbox.ads.adservices.java.measurement.MeasurementManagerFutures;
import androidx.privacysandbox.ads.adservices.measurement.DeletionRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceParams;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceRegistrationRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerParams;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerRegistrationRequest;

import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.url.GURL;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.TimeoutException;

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

    @IntDef({RegistrationType.SOURCE, RegistrationType.TRIGGER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RegistrationType {
        int SOURCE = 0;
        int TRIGGER = 1;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({RegistrationResult.SUCCESS, RegistrationResult.ERROR_UNKNOWN,
            RegistrationResult.ERROR_ILLEGAL_ARGUMENT, RegistrationResult.ERROR_IO,
            RegistrationResult.ERROR_ILLEGAL_STATE, RegistrationResult.ERROR_SECURITY,
            RegistrationResult.ERROR_TIMEOUT, RegistrationResult.ERROR_LIMIT_EXCEEDED,
            RegistrationResult.ERROR_INTERNAL, RegistrationResult.ERROR_BACKGROUND_CALLER,
            RegistrationResult.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface RegistrationResult {
        int SUCCESS = 0;
        int ERROR_UNKNOWN = 1;
        int ERROR_ILLEGAL_ARGUMENT = 2;
        int ERROR_IO = 3;
        int ERROR_ILLEGAL_STATE = 4;
        int ERROR_SECURITY = 5;
        int ERROR_TIMEOUT = 6;
        int ERROR_LIMIT_EXCEEDED = 7;
        int ERROR_INTERNAL = 8;
        int ERROR_BACKGROUND_CALLER = 9;
        int COUNT = 10;
    }

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

    private void onRegistrationCompleted(
            int requestId, @RegistrationType int type, @RegistrationResult int result) {
        switch (type) {
            case RegistrationType.SOURCE:
                RecordHistogram.recordEnumeratedHistogram(
                        "Conversions.AndroidRegistrationResult.Source2", result,
                        RegistrationResult.COUNT);
                break;
            case RegistrationType.TRIGGER:
                RecordHistogram.recordEnumeratedHistogram(
                        "Conversions.AndroidRegistrationResult.Trigger2", result,
                        RegistrationResult.COUNT);

                break;
        }

        if (mNativePtr != 0) {
            AttributionOsLevelManagerJni.get().onRegistrationCompleted(
                    mNativePtr, requestId, result == RegistrationResult.SUCCESS);
        }
    }

    private void addRegistrationFutureCallback(
            int requestId, @RegistrationType int type, ListenableFuture<?> future) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }
        Futures.addCallback(future, new FutureCallback<Object>() {
            @Override
            public void onSuccess(Object result) {
                onRegistrationCompleted(requestId, type, RegistrationResult.SUCCESS);
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "Failed to register", thrown);
                @RegistrationResult
                int result = RegistrationResult.ERROR_UNKNOWN;
                if (thrown instanceof IllegalArgumentException) {
                    result = RegistrationResult.ERROR_ILLEGAL_ARGUMENT;
                } else if (thrown instanceof IOException) {
                    result = RegistrationResult.ERROR_IO;
                } else if (thrown instanceof IllegalStateException) {
                    // The Android API doesn't break out this error as a separate exception so we
                    // are forced to inspect the message for now.
                    if (thrown.getMessage().toLowerCase(Locale.US).contains("background")) {
                        result = RegistrationResult.ERROR_BACKGROUND_CALLER;
                    } else {
                        result = RegistrationResult.ERROR_ILLEGAL_STATE;
                    }
                } else if (thrown instanceof SecurityException) {
                    result = RegistrationResult.ERROR_SECURITY;
                } else if (thrown instanceof TimeoutException) {
                    result = RegistrationResult.ERROR_TIMEOUT;
                } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                        && thrown instanceof LimitExceededException) {
                    result = RegistrationResult.ERROR_LIMIT_EXCEEDED;
                }
                onRegistrationCompleted(requestId, type, result);
            }
        }, ContextUtils.getApplicationContext().getMainExecutor());
    }

    /**
     * Registers a web attribution source with native, see `registerWebSourceAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void registerWebAttributionSource(int requestId, GURL registrationUrl,
            GURL topLevelOrigin, boolean isDebugKeyAllowed, MotionEvent event) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onRegistrationCompleted(
                    requestId, RegistrationType.SOURCE, RegistrationResult.ERROR_INTERNAL);
            return;
        }
        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onRegistrationCompleted(
                    requestId, RegistrationType.SOURCE, RegistrationResult.ERROR_INTERNAL);
            return;
        }
        ListenableFuture<?> future = mm.registerWebSourceAsync(new WebSourceRegistrationRequest(
                Arrays.asList(new WebSourceParams(
                        Uri.parse(registrationUrl.getSpec()), isDebugKeyAllowed)),
                Uri.parse(topLevelOrigin.getSpec()), /*inputEvent=*/event,
                /*appDestination=*/null, /*webDestination=*/null,
                /*verifiedDestination=*/null));
        addRegistrationFutureCallback(requestId, RegistrationType.SOURCE, future);
    }

    /**
     * Registers an attribution source with native, see `registerSourceAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void registerAttributionSource(int requestId, GURL registrationUrl, MotionEvent event) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onRegistrationCompleted(
                    requestId, RegistrationType.SOURCE, RegistrationResult.ERROR_INTERNAL);
            return;
        }
        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onRegistrationCompleted(
                    requestId, RegistrationType.SOURCE, RegistrationResult.ERROR_INTERNAL);
            return;
        }
        ListenableFuture<?> future =
                mm.registerSourceAsync(Uri.parse(registrationUrl.getSpec()), event);
        addRegistrationFutureCallback(requestId, RegistrationType.SOURCE, future);
    }

    /**
     * Registers a web attribution trigger with native, see `registerWebTriggerAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private void registerWebAttributionTrigger(
            int requestId, GURL registrationUrl, GURL topLevelOrigin, boolean isDebugKeyAllowed) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            onRegistrationCompleted(
                    requestId, RegistrationType.TRIGGER, RegistrationResult.ERROR_INTERNAL);
            return;
        }

        MeasurementManagerFutures mm = getManager();
        if (mm == null) {
            onRegistrationCompleted(
                    requestId, RegistrationType.TRIGGER, RegistrationResult.ERROR_INTERNAL);
            return;
        }
        ListenableFuture<?> future = mm.registerWebTriggerAsync(new WebTriggerRegistrationRequest(
                Arrays.asList(new WebTriggerParams(
                        Uri.parse(registrationUrl.getSpec()), isDebugKeyAllowed)),
                Uri.parse(topLevelOrigin.getSpec())));
        addRegistrationFutureCallback(requestId, RegistrationType.TRIGGER, future);
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

        // Currently Android and Chromium have different matching behaviors when both
        // `origins` and `domains` are empty.
        // Chromium: Delete -> Delete nothing; Preserve -> Delete all.
        // Android: Delete -> Delete all; Preserve -> Delete nothing.
        // Android may fix the behavior in the future. As a workaround, Chromium will
        // not call Android if it's to delete nothing (no-op), and call Android with
        // both Delete and Preserve modes if it's to delete all. These two modes will
        // be one no-op and one delete all in Android releases with and without the
        // fix. See crbug.com/1442967.

        ImmutableList<Integer> matchBehaviors = null;

        if (origins.length == 0 && domains.length == 0) {
            switch (matchBehavior) {
                case DeletionRequest.MATCH_BEHAVIOR_DELETE:
                    onDataDeletionCompleted(requestId);
                    return;
                case DeletionRequest.MATCH_BEHAVIOR_PRESERVE:
                    matchBehaviors = ImmutableList.of(DeletionRequest.MATCH_BEHAVIOR_DELETE,
                            DeletionRequest.MATCH_BEHAVIOR_PRESERVE);
                    break;
                default:
                    Log.e(TAG, "Received invalid match behavior: ", matchBehavior);
                    onDataDeletionCompleted(requestId);
                    return;
            }
        } else {
            matchBehaviors = ImmutableList.of(matchBehavior);
        }

        ArrayList<Uri> originUris = new ArrayList<Uri>(origins.length);
        for (GURL origin : origins) {
            originUris.add(Uri.parse(origin.getSpec()));
        }

        ArrayList<Uri> domainUris = new ArrayList<Uri>(domains.length);
        for (String domain : domains) {
            domainUris.add(Uri.parse(domain));
        }

        int numCalls = matchBehaviors.size();

        FutureCallback<Object> callback = new FutureCallback<Object>() {
            private int mNumPendingCalls = numCalls;

            private void onCall() {
                if (--mNumPendingCalls == 0) {
                    onDataDeletionCompleted(requestId);
                }
            }

            @Override
            public void onSuccess(Object result) {
                onCall();
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "Failed to delete measurement API data", thrown);
                onCall();
            }
        };

        for (int currMatchBehavior : matchBehaviors) {
            ListenableFuture<?> future = mm.deleteRegistrationsAsync(new DeletionRequest(
                    deletionMode, currMatchBehavior, Instant.ofEpochMilli(startMs),
                    Instant.ofEpochMilli(endMs), originUris, domainUris));

            Futures.addCallback(
                    future, callback, ContextUtils.getApplicationContext().getMainExecutor());
        }
    }

    /**
     * Gets Measurement API status with native, see `getMeasurementApiStatusAsync()`:
     * https://developer.android.com/reference/androidx/privacysandbox/ads/adservices/java/measurement/MeasurementManagerFutures.
     */
    @CalledByNative
    private static void getMeasurementApiStatus() {
        ThreadUtils.assertOnBackgroundThread();

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
        MeasurementManagerFutures mm =
                MeasurementManagerFutures.from(ContextUtils.getApplicationContext());
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
