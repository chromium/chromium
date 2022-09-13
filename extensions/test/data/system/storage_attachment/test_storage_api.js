// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.storage api test
// extensions_browsertests --gtest_filter=SystemStorageApiTest.Storage

// Testing data should be the same as |kRemovableStorageData| in
// test_storage_info_provider.cc.
var testData = {
  id: "transient:0004",
  name: "/media/usb1",
  type: "removable",
  capacity: 4098
};

var device_id;

chrome.test.runTests([
  function testAttachedEvent() {
    chrome.test.listenOnce(
      chrome.system.storage.onAttached,
      function listener(info) {
        // Record the transient id.
        device_id = info.id;
        chrome.test.assertEq(testData.name, info.name);
        chrome.test.assertEq(testData.type, info.type);
        chrome.test.assertEq(testData.capacity, info.capacity);
      }
    );

    // Tell browser process to attach a new removable storage.
    chrome.test.sendMessage("attach");
  },

  function testDetachedEvent() {
    chrome.test.listenOnce(
      chrome.system.storage.onDetached,
      function listener(id) {
        chrome.test.assertEq(device_id, id);
        chrome.test.sendMessage(id);
      }
    );
    // Tell browser process to detach a storage.
    chrome.test.sendMessage("detach");
  }
]);
