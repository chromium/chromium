// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var device_from_user;

chrome.test.runWithUserGesture(function() {
  chrome.hid.getDevices({}, function(devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(0, devices.length);
    chrome.hid.getUserSelectedDevices({ multiple: false }, function(devices) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(1, devices.length);
      device_from_user = devices[0];
      chrome.test.assertEq(device_from_user.serialNumber, "A");
      chrome.hid.connect(device_from_user.deviceId, function(connection) {
        chrome.test.assertNoLastError();
        chrome.hid.disconnect(connection.connectionId);
        chrome.test.sendMessage("opened_device");
      });
    });
  });
});

chrome.hid.onDeviceRemoved.addListener(function(deviceId) {
  chrome.test.assertEq(device_from_user.deviceId, deviceId);
  chrome.test.sendMessage("removed");
});

chrome.hid.onDeviceAdded.addListener(function(device) {
  chrome.test.assertTrue(device_from_user.deviceId != device.deviceId);
  chrome.test.assertEq(device_from_user.vendorId, device.vendorId);
  chrome.test.assertEq(device_from_user.productId, device.productId);
  chrome.test.sendMessage("added");
});
