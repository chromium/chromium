// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.usb.getDevices({}, devices => {
  if (devices.length !== 1) {
    console.error("Expected a single device, but got " + devices.length + ".");
    chrome.test.sendMessage("failure");
    return;
  }
  device = devices[0];
  if (device.vendorId !== 1 || device.productId !== 2) {
    console.error("Unexpected device was returned by getDevices.");
    chrome.test.sendMessage("failure");
    return;
  }

  chrome.test.sendMessage("ready");
});

chrome.usb.onDeviceRemoved.addListener((device) => {
  if (device.vendorId !== 1 || device.productId !== 2) {
    console.error("Unexpected device was removed.");
    chrome.test.sendMessage("failure");
    return;
  }

  chrome.test.sendMessage("success");
});
