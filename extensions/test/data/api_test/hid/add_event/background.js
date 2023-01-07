// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.hid.onDeviceAdded.addListener(function (device) {
  if (device.vendorId == 6353 && device.productId == 22768) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Got unexpected device: vid:" + device.vendorId +
                  " pid:" + device.productId);
    chrome.test.sendMessage("failure");
  }
});
chrome.test.sendMessage("loaded");
