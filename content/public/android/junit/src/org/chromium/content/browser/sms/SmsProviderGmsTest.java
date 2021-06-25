// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.times;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for SmsProviderGms.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SmsProviderGmsTest {
    private Context mContext;
    private SmsProviderGms mProvider;
    private SmsUserConsentReceiver mUserConsentReceiver;
    private SmsVerificationReceiver mVerificationReceiver;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);

        mUserConsentReceiver = Mockito.mock(SmsUserConsentReceiver.class);
        mVerificationReceiver = Mockito.mock(SmsVerificationReceiver.class);
    }

    private void createSmsProviderGms(@GmsBackend int backend) {
        mProvider = new SmsProviderGms(
                /*native_identifier=*/0, backend, /*isVerificationBackendAvailable=*/true);
        mProvider.setUserConsentReceiverForTesting(mUserConsentReceiver);
        mProvider.setVerificationReceiverForTesting(mVerificationReceiver);
    }

    @Test
    public void testVerificationReceiverCreationWithUserConsentBackend() {
        SmsProviderGms provider = new SmsProviderGms(
                /*native_identifier=*/0, GmsBackend.USER_CONSENT,
                /*isVerificationBackendAvailable=*/true);
        assertNotNull("SmsVerificationReceiver should be created regardless of the backend",
                provider.getVerificationReceiverForTesting());
    }

    @Test
    public void testUserConsentBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.USER_CONSENT);
        boolean isLocalRequest = true;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1)).listen(null);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(0))
                .listen(any(), anyBoolean());
    }

    @Test
    public void testUserConsentBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.USER_CONSENT);
        boolean isLocalRequest = false;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(null, isLocalRequest);
    }

    @Test
    public void testVerificationBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.VERIFICATION);
        boolean isLocalRequest = true;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(null, isLocalRequest);
    }

    @Test
    public void testVerificationBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.VERIFICATION);
        boolean isLocalRequest = false;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(null, isLocalRequest);
    }

    @Test
    public void testAutoBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1)).listen(null);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(null, isLocalRequest);
    }

    @Test
    public void testAutoBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.listen(/*window=*/null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(null, isLocalRequest);
    }

    @Test
    public void testDestroyUserConsentReceiverWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.destoryUserConsentReceiver(isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1)).destroy();
    }

    @Test
    public void testNotDestroyUserConsentReceiverWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.destoryUserConsentReceiver(isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).destroy();
    }

    @Test
    public void testDestroyVerificationReceiverWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.destoryVerificationReceiver(isLocalRequest);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1)).destroy();
    }

    @Test
    public void testNotDestroyVerificationReceiverWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.destoryVerificationReceiver(isLocalRequest);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(0)).destroy();
    }
}
