// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.display.overscan* api test
// extensions_browsertests \
//   --gtest_filter=SystemDisplayApiTest.OverscanCalibrationApp

chrome.test.runTests([
  function testOverscan() {
    var id = "display0";
    chrome.system.display.overscanCalibrationStart(id);
    chrome.system.display.overscanCalibrationAdjust(
        id, {left: 1, top: 1, right: -1, bottom: -1});
    chrome.system.display.overscanCalibrationComplete(id);
    chrome.test.notifyPass();
  }
]);
