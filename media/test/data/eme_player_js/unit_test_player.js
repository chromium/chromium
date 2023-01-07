// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CDM unit test player is used to run a specific test within the CDM. The test
// result is reported via a special key message with UNIT_TEST_RESULT_HEADER
// followed by 1 for success, and 0 for failure.
function UnitTestPlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

UnitTestPlayer.prototype.init = function() {
  // Returns a promise.
  return PlayerUtils.initEMEPlayer(this);
};

UnitTestPlayer.prototype.registerEventListeners = function() {
  // Returns a promise.
  return PlayerUtils.registerEMEEventListeners(this);
};

handleMessage = function(message) {
  // The test result is either '0' or '1' appended to the header.
  var msg = Utils.convertToUint8Array(message.message);
  if (Utils.hasPrefix(msg, UNIT_TEST_RESULT_HEADER)) {
    if (msg.length != UNIT_TEST_RESULT_HEADER.length + 1) {
      Utils.failTest('Unexpected CDM Unit Test message' + msg);
      return;
    }
    var result_index = UNIT_TEST_RESULT_HEADER.length;
    var success = String.fromCharCode(msg[result_index]) == 1;
    Utils.timeLog('CDM unit test: ' + (success ? 'Success' : 'Fail'));
    if (success)
      Utils.setResultInTitle(UNIT_TEST_SUCCESS);
    else
      Utils.failTest(UNIT_TEST_FAILURE);
  }
};

UnitTestPlayer.prototype.onMessage = handleMessage;
