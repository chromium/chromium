// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var socketId;

var onListen = function(result) {
  console.log("Server socket 'listen' completed: sd=" + socketId +
      ", result=" + result);
  chrome.test.assertEq(0, result);
  chrome.test.succeed();
};

var onCreate = function (socketInfo) {
  console.log("Server socket created: sd=" + socketInfo.socketId);
  socketId = socketInfo.socketId;
  chrome.sockets.tcpServer.listen(socketId, '0.0.0.0', 1234, onListen);
};

chrome.test.runTests([
  function bind() {
    chrome.sockets.tcpServer.create({}, onCreate);
  }
]);
