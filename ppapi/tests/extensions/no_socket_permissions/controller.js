// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var control_message;

function testAll() {
  var nacl_module = document.getElementById('nacl_module');
  // The plugin will start the corresponding test and post a message back
  // the test is done. If the test has failed, the message is a description
  // of the error; otherwise the message is empty.
  nacl_module.postMessage(control_message);
}

var onControlMessageReceived = function(message) {
  control_message = message;
  chrome.test.runTests([testAll]);
}

var onPluginMessageReceived = function(message) {
  if (message.data == "PASS") {
    chrome.test.sendMessage("PASS", onControlMessageReceived);
  } else if (message.data) {
    chrome.test.fail(message.data);
  }
};

window.onload = function() {
  var nacl_module = document.getElementById('nacl_module');
  nacl_module.addEventListener("message", onPluginMessageReceived, false);
};
