// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let connectionHandle;

chrome.usb.onDeviceRemoved.addListener(() => {
  chrome.usb.bulkTransfer(connectionHandle, {
    direction: "in",
    endpoint: 1,
    length: 8 }, result => {
      if (chrome.runtime.lastError.message == "No such connection.") {
        chrome.test.sendMessage("success");
        return;
      }

      console.error("Expected transfer failure.");
      chrome.test.sendMessage("failure");
    });
});

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

    connectionHandle = connection;
    chrome.test.sendMessage("ready");
  });
});
