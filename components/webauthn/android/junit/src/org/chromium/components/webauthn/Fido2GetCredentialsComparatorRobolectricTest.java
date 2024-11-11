// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.webauthn.Fido2GetCredentialsComparator.SuccessState;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2GetCredentialsComparatorRobolectricTest {
    private static final String PASSKEY_CACHE_FASTER_HISTOGRAM =
            "WebAuthentication.Android.Fido2VsPasskeyCache.PasskeyCacheFasterMs";
    private static final String FIDO2_FASTER_HISTOGRAM =
            "WebAuthentication.Android.Fido2VsPasskeyCache.Fido2FasterMs";
    private static final String SUCCESS_HISTOGRAM =
            "WebAuthentication.Android.Fido2VsPasskeyCache.SuccessState";

    @Test
    @SmallTest
    public void testFido2SuccessfulThenPasskeyCacheSuccessful() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onGetCredentialsSuccessful(0);
        comparator.onCachedGetCredentialsSuccessful(0);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFido2SuccessfulThenPasskeyCacheFailed() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_FAILED)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onGetCredentialsSuccessful(0);
        comparator.onCachedGetCredentialsFailed();

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFido2FailedThenPasskeyCacheSuccessful() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onGetCredentialsFailed();
        comparator.onCachedGetCredentialsSuccessful(0);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFido2FailedThenPasskeyCacheFailed() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(SUCCESS_HISTOGRAM, SuccessState.FIDO2_FAILED_CACHE_FAILED)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onGetCredentialsFailed();
        comparator.onCachedGetCredentialsFailed();

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testPasskeyCacheSuccessfulThenFido2Successful() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(PASSKEY_CACHE_FASTER_HISTOGRAM)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onCachedGetCredentialsSuccessful(0);
        comparator.onGetCredentialsSuccessful(0);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testPasskeyCacheSuccessfulThenFido2Failed() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_FAILED_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get();

        comparator.onCachedGetCredentialsSuccessful(0);
        comparator.onGetCredentialsFailed();

        watcher.assertExpected();
    }
}
