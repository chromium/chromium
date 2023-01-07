// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let deviceId = null;

const onDeviceChanged = (response) => {
  chrome.hid.getDevices({}, devices => {
    for (let device of devices) {
      if (device.deviceId == deviceId && device.collections.length == 2) {
        chrome.test.sendMessage("changed");
        return;
      }
    }
    console.error("Device not found");
    chrome.test.sendMessage("failure");
  });
};

chrome.hid.onDeviceAdded.addListener(function (device) {
  if (device.collections.length == 1) {
    deviceId = device.deviceId;
    chrome.test.sendMessage("added", onDeviceChanged);
  } else {
    console.error("Got unexpected device with " + device.collections.length +
                  " collections");
    chrome.test.sendMessage("failure");
  }
});

chrome.hid.onDeviceRemoved.addListener(function (removedId) {
  if (deviceId == removedId) {
    chrome.test.sendMessage("success");
  } else {
    console.error("Received removed event for wrong device");
    chrome.test.sendMessage("failure");
  }
});

chrome.test.sendMessage("loaded");
