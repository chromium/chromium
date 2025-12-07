// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!self.postMessage) {
  // This is a shared worker - mimic dedicated worker APIs
  onconnect = function(event) {
    event.ports[0].onmessage = function(e) {
      self.postMessage = function (msg) {
        event.ports[0].postMessage(msg);
      };
      runTests(e.data);
    };
  };
} else {
  runTests(event.data);
}

// This test is compatible with shared-worker-simple.html layout test.
runTests = function (url) {
  var ws;
  var greeting = "hello";
  try {
    ws = new WebSocket(url);
    ws.onopen = function() {
      ws.send(greeting);
    };
    ws.onmessage = function(e) {
      // Receive echoed "hello".
      if (e.data != greeting) {
        postMessage("FAIL: received data is wrong: " + e.data);
      } else {
        ws.close();
      }
    };
    ws.onclose = function(e) {
      if (!e.wasClean) {
        postMessage("FAIL: close is not clean");
      } else {
        postMessage("DONE");
      }
    };
  } catch (e) {
    postMessage("FAIL: worker: Unexpected exception: " + e);
  }
};
