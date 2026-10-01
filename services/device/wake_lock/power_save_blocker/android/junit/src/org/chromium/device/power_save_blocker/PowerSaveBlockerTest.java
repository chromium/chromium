// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.power_save_blocker;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.Manifest;
import android.content.Context;
import android.os.PowerManager;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.device.mojom.WakeLockType;

/** Unit tests for {@link PowerSaveBlocker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PowerSaveBlockerTest {
    private Context mContext;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mShadowApplication = Shadows.shadowOf(RuntimeEnvironment.getApplication());
        ShadowPowerManager.reset();
    }

    @Test
    public void testApplyAndRemoveViewBlock() {
        View view = new View(mContext);
        assertFalse(view.getKeepScreenOn());

        PowerSaveBlocker blocker1 = PowerSaveBlocker.create();
        PowerSaveBlocker blocker2 = PowerSaveBlocker.create();

        blocker1.applyBlock(view);
        assertTrue(view.getKeepScreenOn());

        blocker2.applyBlock(view);
        assertTrue(view.getKeepScreenOn());

        blocker1.removeBlock();
        assertTrue(view.getKeepScreenOn());

        blocker2.removeBlock();
        assertFalse(view.getKeepScreenOn());
    }

    @Test
    public void testApplyAndRemoveWakeLockWithPermission() {
        mShadowApplication.grantPermissions(Manifest.permission.WAKE_LOCK);

        int[] wakeLockTypes =
                new int[] {
                    WakeLockType.PREVENT_APP_SUSPENSION,
                    WakeLockType.PREVENT_DISPLAY_SLEEP,
                    WakeLockType.PREVENT_DISPLAY_SLEEP_ALLOW_DIMMING
                };

        for (int type : wakeLockTypes) {
            ShadowPowerManager.reset();
            PowerSaveBlocker blocker = PowerSaveBlocker.create();
            blocker.applyWakeLock(type);

            PowerManager.WakeLock wakeLock = ShadowPowerManager.getLatestWakeLock();
            assertNotNull(wakeLock);
            assertTrue(wakeLock.isHeld());

            blocker.removeBlock();
            assertFalse(wakeLock.isHeld());
        }
    }

    @Test
    public void testApplyWakeLockWithoutPermissionDoesNotAcquire() {
        mShadowApplication.denyPermissions(Manifest.permission.WAKE_LOCK);

        PowerSaveBlocker blocker = PowerSaveBlocker.create();
        blocker.applyWakeLock(WakeLockType.PREVENT_DISPLAY_SLEEP);

        assertNull(ShadowPowerManager.getLatestWakeLock());
        blocker.removeBlock();
    }
}
