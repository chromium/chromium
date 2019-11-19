// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
onmessage = async () => {
  let devices = await navigator.usb.getDevices();
  postMessage("Found " + devices.length + " devices");
}
