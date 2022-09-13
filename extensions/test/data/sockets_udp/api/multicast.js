// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for multicast UDP chrome.sockets.udp.
function testMulticast() {
  function randomHexString(count) {
    var result = '';
    for (var i = 0; i < count; i++) {
      result += (Math.random() * 16 >> 0).toString(16);
    }
    return result;
  }

  var kMulticastAddress = "237.132.100.133";
  var kTestMessageLength = 128;
  var kTestMessage = randomHexString(128);
  var kPort = 11103;

  function arrayBufferToString(arrayBuffer) {
    // UTF-16LE
    return String.fromCharCode.apply(String, new Uint16Array(arrayBuffer));
  }

  function stringToArrayBuffer(string) {
    // UTF-16LE
    var buf = new ArrayBuffer(string.length * 2);
    var bufView = new Uint16Array(buf);
    for (var i = 0, strLen = string.length; i < strLen; i++) {
      bufView[i] = string.charCodeAt(i);
    }
    return buf;
  }

  // Registers a listener on receiving data on |socketId|.
  // Calls |callback| with a |cancelled| argument of "false" when receiving data
  // of excactly |kTestMessageLength| characters.
  // Calls |callback| with a |cancelled| argument of "true" when the caller
  // decides to invoke the returned function.
  // Returns a function that can be invoked to "cancel" the operation (i.e.
  // call |callback| with a |cancelled| argument of "true").
  function waitForMessage(socketId, callback) {
    var cancelled = false;
    var relayCanceller = null;
    chrome.sockets.udp.onReceive.addListener(function(info) {
      console.log("Data received: " +
          "socketId=" + info.socketId +
          ", bytes=" + info.data.byteLength +
          ", address=" + info.remoteAddress +
          ", port=" + info.remotePort);
      if (socketId != info.socketId)
        return;

      if (cancelled)
        return;

      if (info.data.byteLength == kTestMessageLength * 2 &&
          kTestMessage === arrayBufferToString(info.data)) {
        callback(false);
      } else {
        // Restart waiting.
        relayCanceller = waitForMessage(socketId, callback);
      }
    });
    return function canceller() {
      if (relayCanceller) {
        relayCanceller();
      } else {
        cancelled = true;  // prevents callback from being called on receive.
        callback(true);
      }
    };
  }

  function testMulticastSettings(nextTest) {
    console.log("*************** testMulticastSettings");
    chrome.sockets.udp.create({}, function (socketInfo) {
      var socketId;
      if (socketInfo) {
        socketId = socketInfo.socketId;
        chrome.sockets.udp.setMulticastTimeToLive(socketId, 0,
            function (result) {
          chrome.test.assertEq(0, result,
              "Error setting multicast time to live.");
          chrome.sockets.udp.setMulticastTimeToLive(socketId, -3,
              function (result) {
            chrome.test.assertEq(-4, result,
                "Error setting multicast time to live.");
            chrome.sockets.udp.setMulticastLoopbackMode(socketId, false,
                function (result) {
              chrome.test.assertEq(0, result,
                  "Error setting multicast loop back mode.");
              chrome.sockets.udp.setMulticastLoopbackMode(socketId, true,
                  function (result) {
                chrome.test.assertEq(0, result,
                    "Error setting multicast loop back mode.");
                chrome.sockets.udp.close(socketId, function() {});
                nextTest();
              });
            });
          });
        });
      } else {
        chrome.test.fail("Cannot create server udp socket");
      }
    });
  }

  function testSendMessage(message, address) {
    // Send the UDP message to the address with multicast ttl = 0.
    chrome.sockets.udp.create({}, function (socketInfo) {
      var clientSocketId = socketInfo.socketId;
      chrome.test.assertTrue(clientSocketId > 0,
          "Cannot create client udp socket.");
      chrome.sockets.udp.setMulticastTimeToLive(clientSocketId, 0,
          function (result) {
        chrome.test.assertEq(0, result,
            "Cannot create client udp socket.");
        chrome.sockets.udp.bind(clientSocketId, "0.0.0.0", 0,
            function (result) {
          chrome.test.assertEq(0, result,
            "Cannot bind to localhost.");
          chrome.sockets.udp.send(clientSocketId,
              stringToArrayBuffer(kTestMessage),
              address, kPort, function (result) {
            console.log("Sent bytes to socket:" +
                " socketId=" + clientSocketId +
               ", bytes=" + result.bytesSent +
               ", address=" + address +
               ", port=" + kPort);
            chrome.test.assertTrue(result.resultCode >= 0,
                "Send to failed. " + JSON.stringify(result));
            chrome.sockets.udp.close(clientSocketId, function() {});
          });
        });
      });
    });
  }

  function testRecvBeforeAddMembership(serverSocketId, nextTest) {
    console.log("*************** testRecvBeforeAddMembership");
    var recvTimeout;
    var canceller = waitForMessage(serverSocketId, function (cancelled) {
      clearTimeout(recvTimeout);
      if (cancelled) {
        nextTest();
      } else {
        chrome.test.fail("Received message before joining the group");
      }
    });
    testSendMessage(kTestMessage, kMulticastAddress); // Meant to be lost.
    recvTimeout = setTimeout(function () {
      // This is expected to execute.
      canceller();
    }, 2000);
  }

  function testRecvWithMembership(serverSocketId, nextTest) {
    console.log("*************** testRecvWithMembership");
    chrome.sockets.udp.joinGroup(serverSocketId, kMulticastAddress,
        function (result) {
      chrome.test.assertEq(0, result, "Join group failed.");
      var recvTimeout;
      var canceller = waitForMessage(serverSocketId, function (cancelled) {
        clearTimeout(recvTimeout);
        if (!cancelled) {
          nextTest();
        } else {
          chrome.test.fail("Faild to receive message after joining the group");
        }
      });
      testSendMessage(kTestMessage, kMulticastAddress);
      recvTimeout = setTimeout(function () {
        canceller();
        chrome.test.fail("Cannot receive from multicast group.");
      }, 2000);
    });
  }

  function testRecvWithoutMembership(serverSocketId, nextTest) {
    console.log("*************** testRecvWithoutMembership");
    chrome.sockets.udp.leaveGroup(serverSocketId, kMulticastAddress,
        function (result) {
      chrome.test.assertEq(0, result, "leave group failed.");
      var recvTimeout;
      var canceller = waitForMessage(serverSocketId, function (cancelled) {
        clearTimeout(recvTimeout);
        if (cancelled) {
          nextTest();
        } else {
          chrome.test.fail("Received message after leaving the group");
        }
      });
      testSendMessage(request, kMulticastAddress);
      recvTimeout = setTimeout(function () {
        // This is expected to execute.
        canceller();
      }, 2000);
    });
  }

  function testMulticastRecv() {
    console.log("*************** testMulticastRecv");
    chrome.sockets.udp.create({}, function (socketInfo) {
      var serverSocketId = socketInfo.socketId;
      chrome.sockets.udp.bind(serverSocketId, "0.0.0.0", kPort,
          function (result) {
        chrome.test.assertEq(0, result, "Bind failed.");
        // Test 1
        testRecvBeforeAddMembership(serverSocketId, function() {
          // Test 2
          testRecvWithMembership(serverSocketId, function() {
            // Test 3
            testRecvWithoutMembership(serverSocketId, function() {
              // Success!
              chrome.sockets.udp.close(serverSocketId, function() {
                console.log("*************** SUCCESS! ");
                chrome.test.succeed();
              });
            });
          });
        });
      });
    });
  }

  setTimeout(function() {
    testMulticastSettings(function() {
      testMulticastRecv();
    })
  }, 100);
}
