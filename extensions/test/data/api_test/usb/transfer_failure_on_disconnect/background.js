// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.usb.getDevices({}, devices => {
  if (devices.length !== 1) {
    console.error("Expected a single device.");
    chrome.test.sendMessage("failure");
  }
  device = devices[0];
  chrome.usb.openDevice(device, connection => {
    if (connection === undefined) {
      console.error("Failed to open device.");
      chrome.test.sendMessage("failure");
      return;
    }

    chrome.usb.bulkTransfer(connection, {
      direction: "in",
      endpoint: 1,
      length: 8 }, result => {
        if (chrome.runtime.lastError.message == "Device disconnected.") {
          chrome.test.sendMessage("success");
          return;
        }

        console.error("Expected transfer failure.");
        chrome.test.sendMessage("failure");
      });

    chrome.test.sendMessage("ready");
  });
});
