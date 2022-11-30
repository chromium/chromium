// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Basic tests of API functions in a real (non-mocked) shell to
 * ensure they can be called from a particular client or environment.
 * Tests that verify specific behaviors should be in their own API tests.
 */

// Tests chrome.bluetooth availability.
function bluetoothSanityCheck() {
  chrome.test.assertTrue(
    !!chrome.bluetooth, 'chrome.bluetooth should be available');

  chrome.bluetooth.getAdapterState(chrome.test.callback());
  chrome.bluetooth.getDevice(
      'AB:CD:EF:01:23:45', chrome.test.callbackFail('Invalid device'));
  chrome.bluetooth.getDevices(chrome.test.callback());

  var startDiscoveryCallback = chrome.test.callbackAdded();
  chrome.bluetooth.startDiscovery(function() {
    // Ignore errors.
    chrome.runtime.lastError;
    startDiscoveryCallback();
  });

  var stopDiscoveryCallback = chrome.test.callbackAdded();
  chrome.bluetooth.stopDiscovery(function() {
    // Ignore errors.
    chrome.runtime.lastError;
    stopDiscoveryCallback();
  });
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([bluetoothSanityCheck]);
});
