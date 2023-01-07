// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var known_devices = {};

chrome.hid.onDeviceRemoved.addListener(function (deviceId) {
  if (deviceId in known_devices) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Unexpected device removed: " + device.deviceId);
    chrome.test.sendMessage("failure");
  }
});

chrome.hid.getDevices({}, function (devices) {
  for (var device of devices) {
    known_devices[device.deviceId] = device;
  }
  chrome.test.sendMessage("loaded");
});
