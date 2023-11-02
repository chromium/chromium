// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function getDevices() {
  usb.getDevices({
      vendorId: 0,
      productId: 0
  }, function(devices) {
    chrome.test.assertEq(1, devices.length);
    var device = devices[0];
    chrome.test.assertEq(0x0100, device.version);
    chrome.test.assertEq("Test Device", device.productName);
    chrome.test.assertEq("Test Manufacturer", device.manufacturerName);
    chrome.test.assertEq("ABC123", device.serialNumber);
    usb.openDevice(device, function(handle) {
      chrome.test.assertNoLastError();
      usb.closeDevice(handle);
      chrome.test.succeed();
    });
  });
}

function getConfigurations() {
  usb.getDevices({}, function(devices) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, devices.length);
    chrome.usb.getConfigurations(devices[0], function(configs) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(2, configs.length);
      chrome.test.assertTrue(configs[0].active);
      chrome.test.assertEq(1, configs[0].configurationValue);
      chrome.test.assertFalse(configs[1].active);
      chrome.test.assertEq(2, configs[1].configurationValue);
      chrome.test.succeed();
    });
  });
}

function explicitCloseDevice() {
  usb.findDevices({
      vendorId: 0,
      productId: 0
  }, function(devices) {
    usb.closeDevice(devices[0]);
    chrome.test.succeed();
  });
}

chrome.test.runTests([
    getDevices, getConfigurations, explicitCloseDevice
]);
