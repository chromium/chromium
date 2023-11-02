// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var attachedDeviceId;

function testAttach(details) {
  attachedDeviceId = details.id;
  chrome.test.sendMessage('attach_test_ok,' + details.name);
};

function ejectCallback(result) {
  if (result == "success") {
    chrome.test.sendMessage("eject_ok");
  } else if (result == "in_use") {
    chrome.test.sendMessage("eject_in_use");
  } else if (result == "no_such_device") {
    chrome.test.sendMessage("eject_no_such_device");
  } else {
    chrome.test.sendMessage("eject_failure");
  }
};

function ejectTest() {
  chrome.system.storage.ejectDevice(attachedDeviceId, ejectCallback);
};

function addAttachListener() {
  chrome.system.storage.onAttached.addListener(testAttach);
  chrome.test.sendMessage('add_attach_ok');
};

function removeAttachListener() {
  chrome.system.storage.onAttached.removeListener(testAttach);
  chrome.test.sendMessage('remove_attach_ok');
};

function ejectFailTest() {
  chrome.system.storage.ejectDevice('-1', ejectCallback);
};

chrome.test.sendMessage('loaded');
