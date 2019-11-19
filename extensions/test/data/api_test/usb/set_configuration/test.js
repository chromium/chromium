// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var tests = [
  function setConfiguration() {
    usb.findDevices({vendorId: 0, productId: 0}, function (devices) {
      var device = devices[0];
      usb.getConfiguration(device, function (result) {
        chrome.test.assertLastError("The device is not in a configured state.");
        usb.setConfiguration(device, 1, function () {
          chrome.test.assertNoLastError();
          usb.getConfiguration(device, function (result) {
            chrome.test.assertNoLastError();
            usb.closeDevice(device);
            chrome.test.succeed();
          });
        });
      });
    });
  }
];

chrome.test.runTests(tests);
