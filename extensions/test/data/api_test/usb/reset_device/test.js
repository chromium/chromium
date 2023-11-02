// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function resetDevice() {
  usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
    usb.resetDevice(devices[0], function(result) {
      chrome.test.assertEq(result, true);
      // Ensure the device is still open.
      var transfer = {
        direction: "out",
        endpoint: 2,
        data: new ArrayBuffer(1)
      };
      usb.interruptTransfer(devices[0], transfer, function(result) {
        // This is designed to fail.
        usb.resetDevice(devices[0], function(result) {
          chrome.test.assertLastError(
              'Error resetting the device. The device has been closed.');
          chrome.test.assertEq(false, result);
          usb.interruptTransfer(devices[0], transfer, function(result) {
            chrome.test.assertEq(undefined, result);
            chrome.test.assertLastError('No such connection.');
            chrome.test.succeed();
          });
        });
      });
    });
  });
}

chrome.test.runTests([resetDevice]);
