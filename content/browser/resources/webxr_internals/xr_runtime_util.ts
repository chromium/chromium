// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {XRDeviceId} from './xr_device.mojom-webui.js';

export function deviceIdToString(deviceId: XRDeviceId): string {
  switch (deviceId) {
    case XRDeviceId.WEB_TEST_DEVICE_ID:
      return 'Web Test Device';
    case XRDeviceId.FAKE_DEVICE_ID:
      return 'Fake Device';
    case XRDeviceId.ORIENTATION_DEVICE_ID:
      return 'Orientation Device';
    // <if expr="enable_arcore">
    case XRDeviceId.ARCORE_DEVICE_ID:
      return 'ARCore Device';
    // </if>
    // <if expr="enable_openxr">
    case XRDeviceId.OPENXR_DEVICE_ID:
      return 'OpenXR Device';
    // </if>
    // <if expr="enable_cardboard">
    case XRDeviceId.CARDBOARD_DEVICE_ID:
      return 'Cardboard Device';
    // </if>
    default:
      return '';
  }
}
