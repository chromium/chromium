// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var device_from_user = undefined;

chrome.test.runWithUserGesture(function() {
  chrome.usb.getDevices({}, function(devices) {
    chrome.test.assertEq(0, devices.length);
    chrome.usb.getUserSelectedDevices({ multiple: false }, function(devices) {
      chrome.test.assertEq(1, devices.length);
      device_from_user = devices[0];
      chrome.usb.openDevice(device_from_user, function(connection) {
        chrome.usb.closeDevice(connection);
        chrome.test.sendMessage("opened_device");
      });
    });
  });
});

chrome.usb.onDeviceRemoved.addListener(function(device) {
  if (device.device == device_from_user.device) {
    chrome.test.sendMessage("success");
  } else {
    chrome.test.sendMessage("failure");
  }
});
