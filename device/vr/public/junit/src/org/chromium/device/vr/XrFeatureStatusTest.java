// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.vr;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.PackageManagerUtils.XR_OPENXR_FEATURE_NAME;

import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link XrFeatureStatus} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class XrFeatureStatusTest {
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void beforeTest() {
        PackageManager packageManager = RuntimeEnvironment.getApplication().getPackageManager();
        mShadowPackageManager = Shadow.extract(packageManager);
    }

    @Test
    public void isXrDevice_systemHasOpenXrFeature_isTrue() {
        mShadowPackageManager.setSystemFeature(XR_OPENXR_FEATURE_NAME, true);
        assertTrue(
                "Device supports " + XR_OPENXR_FEATURE_NAME + " should be XR device.",
                XrFeatureStatus.isXrDevice());
    }

    @Test
    public void isXrDevice_systemDoesNotHaveOpenXrFeature_isFalse() {
        mShadowPackageManager.setSystemFeature(XR_OPENXR_FEATURE_NAME, false);
        assertFalse(
                "Only device supports " + XR_OPENXR_FEATURE_NAME + " is XR device.",
                XrFeatureStatus.isXrDevice());
    }
}
