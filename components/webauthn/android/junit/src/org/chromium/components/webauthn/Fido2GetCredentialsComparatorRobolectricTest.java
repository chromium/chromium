// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.webauthn.Fido2GetCredentialsComparator.SuccessState;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class Fido2GetCredentialsComparatorRobolectricTest {
    private static final String HISTOGRAM_BASE = "WebAuthentication.Android.Fido2VsPasskeyCache";
    private static final String PASSKEY_CACHE_COUNT_HISTOGRAM =
            HISTOGRAM_BASE + ".PasskeyCacheCredentialCountWhenDifferent";
    private static final String PASSKEY_CACHE_FASTER_HISTOGRAM =
            HISTOGRAM_BASE + ".PasskeyCacheFasterMs";
    private static final String FIDO2_COUNT_HISTOGRAM =
            HISTOGRAM_BASE + ".Fido2CredentialCountWhenDifferent";
    private static final String FIDO2_FASTER_HISTOGRAM = HISTOGRAM_BASE + ".Fido2FasterMs";
    private static final String SUCCESS_HISTOGRAM = HISTOGRAM_BASE + ".SuccessState";
    private static final String COUNT_DIFFERENCE_HISTOGRAM =
            HISTOGRAM_BASE + ".CredentialCountDifference";
    private static final String GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM =
            HISTOGRAM_BASE + ".CredentialCountDifference.GoogleRp";
    private static final String NON_GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM =
            HISTOGRAM_BASE + ".CredentialCountDifference.NonGoogleRp";

    @Test
    @SmallTest
    public void testFido2SuccessfulThenPasskeyCacheSuccessfulNonGoogleRp() {
        int fido2CredentialCount = 1;
        int passkeyCacheCredentialCount = 1;
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM)
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM + ".NonGoogleRp")
                        .expectNoRecords(PASSKEY_CACHE_FASTER_HISTOGRAM)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .expectIntRecord(COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM)
                        .expectIntRecord(NON_GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(FIDO2_COUNT_HISTOGRAM)
                        .expectNoRecords(PASSKEY_CACHE_COUNT_HISTOGRAM)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

        comparator.onGetCredentialsSuccessful(fido2CredentialCount);
        Robolectric.getForegroundThreadScheduler().advanceBy(100, TimeUnit.MILLISECONDS);
        comparator.onCachedGetCredentialsSuccessful(passkeyCacheCredentialCount);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFido2SuccessfulThenPasskeyCacheSuccessfulGoogleRp() {
        int fido2CredentialCount = 1;
        int passkeyCacheCredentialCount = 1;
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM)
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM + ".GoogleRp")
                        .expectNoRecords(PASSKEY_CACHE_FASTER_HISTOGRAM)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .expectIntRecord(COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(NON_GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM)
                        .expectIntRecord(GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(FIDO2_COUNT_HISTOGRAM)
                        .expectNoRecords(PASSKEY_CACHE_COUNT_HISTOGRAM)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ true);

        comparator.onGetCredentialsSuccessful(fido2CredentialCount);
        Robolectric.getForegroundThreadScheduler().advanceBy(100, TimeUnit.MILLISECONDS);
        comparator.onCachedGetCredentialsSuccessful(passkeyCacheCredentialCount);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testFido2SuccessfulThenPasskeyCacheSuccessfulWithCountDifference() {
        int fido2CredentialCount = 0;
        int passkeyCacheCredentialCount = 2;
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(FIDO2_FASTER_HISTOGRAM)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .expectIntRecord(FIDO2_COUNT_HISTOGRAM, fido2CredentialCount)
                        .expectIntRecord(PASSKEY_CACHE_COUNT_HISTOGRAM, passkeyCacheCredentialCount)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

        comparator.onGetCredentialsSuccessful(fido2CredentialCount);
        Robolectric.getForegroundThreadScheduler().advanceBy(100, TimeUnit.MILLISECONDS);
        comparator.onCachedGetCredentialsSuccessful(passkeyCacheCredentialCount);

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
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

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
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

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
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

        comparator.onGetCredentialsFailed();
        comparator.onCachedGetCredentialsFailed();

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testPasskeyCacheSuccessfulThenFido2SuccessfulNonGoogleRp() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(PASSKEY_CACHE_FASTER_HISTOGRAM)
                        .expectNoRecords(FIDO2_FASTER_HISTOGRAM)
                        .expectIntRecord(COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM)
                        .expectIntRecord(NON_GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

        comparator.onCachedGetCredentialsSuccessful(0);
        Robolectric.getForegroundThreadScheduler().advanceBy(100, TimeUnit.MILLISECONDS);
        comparator.onGetCredentialsSuccessful(0);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testPasskeyCacheSuccessfulThenFido2SuccessfulGoogleRp() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(PASSKEY_CACHE_FASTER_HISTOGRAM)
                        .expectNoRecords(FIDO2_FASTER_HISTOGRAM)
                        .expectIntRecord(COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectNoRecords(NON_GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM)
                        .expectIntRecord(GOOGLE_RP_COUNT_DIFFERENCE_HISTOGRAM, 0)
                        .expectIntRecord(
                                SUCCESS_HISTOGRAM, SuccessState.FIDO2_SUCCESSFUL_CACHE_SUCCESSFUL)
                        .build();
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ true);

        comparator.onCachedGetCredentialsSuccessful(0);
        Robolectric.getForegroundThreadScheduler().advanceBy(100, TimeUnit.MILLISECONDS);
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
        var comparator = Fido2GetCredentialsComparator.Factory.get(/* isGoogleRp= */ false);

        comparator.onCachedGetCredentialsSuccessful(0);
        comparator.onGetCredentialsFailed();

        watcher.assertExpected();
    }
}
