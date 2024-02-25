// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.sms;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for SmsProviderGms. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SmsProviderGmsTest {
    private Context mContext;
    private SmsProviderGms mProvider;
    private SmsUserConsentReceiver mUserConsentReceiver;
    private SmsVerificationReceiver mVerificationReceiver;
    private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;

        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mContext));

        mUserConsentReceiver = Mockito.mock(SmsUserConsentReceiver.class);
        mVerificationReceiver = Mockito.mock(SmsVerificationReceiver.class);
    }

    private void createSmsProviderGms(@GmsBackend int backend) {
        mProvider =
                new SmsProviderGms(
                        /* native_identifier= */ 0,
                        backend,
                        /* isVerificationBackendAvailable= */ true);
        mProvider.setUserConsentReceiverForTesting(mUserConsentReceiver);
        mProvider.setVerificationReceiverForTesting(mVerificationReceiver);
    }

    @Test
    public void testVerificationReceiverCreationWithUserConsentBackend() {
        SmsProviderGms provider =
                new SmsProviderGms(
                        /* native_identifier= */ 0,
                        GmsBackend.USER_CONSENT,
                        /* isVerificationBackendAvailable= */ true);
        assertNotNull(
                "SmsVerificationReceiver should be created regardless of the backend",
                provider.getVerificationReceiverForTesting());
    }

    @Test
    public void testUserConsentBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.USER_CONSENT);
        boolean isLocalRequest = true;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1))
                .listen(mWindowAndroid);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(0))
                .listen(anyBoolean());
    }

    @Test
    public void testUserConsentBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.USER_CONSENT);
        boolean isLocalRequest = false;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testVerificationBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.VERIFICATION);
        boolean isLocalRequest = true;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testVerificationBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.VERIFICATION);
        boolean isLocalRequest = false;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testAutoBackendWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1))
                .listen(mWindowAndroid);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testAutoBackendWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.listen(mWindowAndroid, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(any());
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testDestroyUserConsentReceiverWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.verificationReceiverSucceeded(isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(1)).destroy();
    }

    @Test
    public void testNotDestroyUserConsentReceiverWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.verificationReceiverSucceeded(isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).destroy();
    }

    @Test
    public void testDestroyVerificationReceiverWithLocalRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.verificationReceiverFailed(isLocalRequest);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1)).destroy();
    }

    @Test
    public void testNotDestroyVerificationReceiverWithRemoteRequest() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = false;
        mProvider.verificationReceiverFailed(isLocalRequest);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(0)).destroy();
    }

    @Test
    public void testUserConsentBackendWithoutWindow() {
        createSmsProviderGms(GmsBackend.USER_CONSENT);
        boolean isLocalRequest = true;
        mProvider.listen(/* window= */ null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(null);
    }

    @Test
    public void testVerificationBackendWithoutWindow() {
        createSmsProviderGms(GmsBackend.VERIFICATION);
        boolean isLocalRequest = true;
        mProvider.listen(/* window= */ null, isLocalRequest);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }

    @Test
    public void testAutoBackendWithoutWindow() {
        createSmsProviderGms(GmsBackend.AUTO);
        boolean isLocalRequest = true;
        mProvider.listen(/* window= */ null, isLocalRequest);
        Mockito.verify(mProvider.getUserConsentReceiverForTesting(), times(0)).listen(null);
        Mockito.verify(mProvider.getVerificationReceiverForTesting(), times(1))
                .listen(isLocalRequest);
    }
}
