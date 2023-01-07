// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var known_devices = {};

chrome.usb.onDeviceRemoved.addListener(function(device) {
  if (device.device in known_devices) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Unexpected device removed: " + device.device);
    chrome.test.sendMessage("failure");
  }
});

chrome.usb.getDevices({}, function(devices) {
  for (var device of devices) {
    known_devices[device.device] = device;
  }
  chrome.test.sendMessage("loaded");
});
