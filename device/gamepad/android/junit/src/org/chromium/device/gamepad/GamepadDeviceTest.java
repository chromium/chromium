// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Verify no regressions in gamepad mappings. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GamepadDeviceTest {
    @Test
    @Feature({"Gamepad"})
    public void testRelevantKeycodesAreSorted() {
        for (int i = 0; i < GamepadDevice.RELEVANT_KEYCODES.length - 1; ++i) {
            Assert.assertTrue(
                    GamepadDevice.RELEVANT_KEYCODES[i] < GamepadDevice.RELEVANT_KEYCODES[i + 1]);
        }
    }
}
