// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.device_posture;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link DevicePosturePlatformProviderAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.TIRAMISU)
@EnableFeatures(BlinkFeatures.DEVICE_POSTURE)
public class DevicePosturePlatformProviderAndroidTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContentsImpl mWebContents;
    @Mock private WindowAndroid mWindowAndroid;

    private DevicePosturePlatformProviderAndroid mProvider;

    @Before
    public void setUp() {
        mProvider = DevicePosturePlatformProviderAndroid.create(1L, mWebContents);
    }

    @Test
    public void testStartListeningSkipsWhenNotFoldable() {
        DeviceInfo.setIsFoldableForTesting(false);
        mProvider.startListening();
        verify(mWebContents, never()).getTopLevelNativeWindow();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testStartListeningSkipsBelowMinSdk() {
        DeviceInfo.setIsFoldableForTesting(true);
        mProvider.startListening();
        verify(mWebContents, never()).getTopLevelNativeWindow();
    }

    @Test
    public void testStartListeningObservesWhenFoldable() {
        DeviceInfo.setIsFoldableForTesting(true);
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        mProvider.startListening();
        verify(mWebContents).getTopLevelNativeWindow();
    }
}
