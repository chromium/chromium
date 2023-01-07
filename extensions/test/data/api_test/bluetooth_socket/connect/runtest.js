// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var address = '11:12:13:14:15:16';
var uuid = '8e3ad063-db38-4289-aa8f-b30e4223cf40';

function testConnect() {
  chrome.test.assertEq(1, sockets.length);
  chrome.test.assertEq(socketId, sockets[0].socketId);
  chrome.test.assertEq(false, sockets[0].persistent);
  chrome.test.assertEq(undefined, sockets[0].name);
  chrome.test.assertEq(false, sockets[0].paused);
  chrome.test.assertEq(true, sockets[0].connected);
  chrome.test.assertEq(address, sockets[0].address);
  chrome.test.assertEq(uuid, sockets[0].uuid);

  chrome.test.succeed();
}

function startTests() {
  chrome.test.runTests([testConnect]);
}

function expectError(message) {
  if (!chrome.runtime.lastError) {
    chrome.test.fail("Expected an error");
  }
  chrome.test.assertEq(message, chrome.runtime.lastError.message);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

function createConnectedSocket(address, uuid, callback) {
  chrome.bluetoothSocket.create(
    function(socket) {
      failOnError();
      chrome.bluetoothSocket.connect(
        socket.socketId, address, uuid,
        function() {
          callback(socket);
        });
    });
}

function runSocketErrorTests(callback) {
  chrome.bluetoothSocket.connect(1234, address, uuid,
    function() {
      expectError("Socket not found");

      createConnectedSocket('aa:aa:aa:aa:aa:aa', uuid,
        function(socket) {
          expectError("Device not found");

          createConnectedSocket(address, 'not a valid uuid',
            function(socket) {
              expectError("Invalid UUID");

              createConnectedSocket(address, '1234',
                function(socket) {
                  expectError("Permission denied");

                  callback();
                });
            });
        });
    });
}

createConnectedSocket(address, uuid,
  function(socket) {
    failOnError();

    // Make sure that the socket appears in the sockets list.
    chrome.bluetoothSocket.getSockets(
      function(result) {
        failOnError();
        sockets = result;
        socketId = socket.socketId;

        // Run some error checks.
        runSocketErrorTests(
          function() {
            chrome.bluetoothSocket.disconnect(socket.socketId);
            chrome.test.sendMessage('ready', startTests);
          });
      });
  });
