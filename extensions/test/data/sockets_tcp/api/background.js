// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use an HTTP server configured to echo back the request body as the response
// body.
const httpPost =
    "POST /echo HTTP/1.1\r\n" +
    "Content-Length: 19\r\n\r\n" +
    "0100000005320000005";
var expectedResponsePattern = /\n0100000005320000005$/;

// The echo server can send back the response in multiple chunks. We must wait
// for at least `minExpectedResponseLength` bytes to be received before matching
// the response with `expectedResponsePattern`.
var minExpectedResponseLength = 102;

var tcp_address;
var https_address;
var bytesSent = 0;
var dataAsString = "";
var dataRead = [];
var tcp_port = -1;
var https_port = -1;
var protocol = "none";
var tcp_socketId = 0;
var https_socketId = 0;
var echoDataSent = false;
var waitCount = 0;

// Keys are socketIds. Values are inner dictionaries with two keys: onReceive,
// onReceiveError. Both are functions.
var receive_dispatcher = {}

// Many thanks to Dennis for the StackOverflow answer: http://goo.gl/UDanx
// Since amended to handle BlobBuilder deprecation.
function string2ArrayBuffer(string, callback) {
  var blob = new Blob([string]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsArrayBuffer(blob);
}

function arrayBuffer2String(buf, callback) {
  var blob = new Blob([new Uint8Array(buf)]);
  var f = new FileReader();
  f.onload = function(e) {
    callback(e.target.result);
  };
  f.readAsText(blob);
}

function dispatchSocketReceive(receiveInfo) {
  if (receive_dispatcher[receiveInfo.socketId] !== undefined) {
    receive_dispatcher[receiveInfo.socketId].onReceive(receiveInfo);
  } else {
    console.log("dispatchSocketReceive: No handler for socket " +
        receiveInfo.socketId);
  }
}

function dispatchSocketReceiveError(receiveErrorInfo) {
  if (receive_dispatcher[receiveErrorInfo.socketId] !== undefined) {
    receive_dispatcher[receiveErrorInfo.socketId].onReceiveError(
        receiveErrorInfo);
  } else {
    console.log("dispatchSocketReceiveError: No handler for socket " +
        receiveErrorInfo.socketId);
  }
}

var testSocketCreation = function() {
  function onCreate(socketInfo) {
    function onGetInfo(info) {
      chrome.test.assertFalse(info.connected);

      if (info.peerAddress || info.peerPort) {
        chrome.test.fail('Unconnected socket should not have peer');
      }
      if (info.localAddress || info.localPort) {
        chrome.test.fail('Unconnected socket should not have local binding');
      }

      chrome.sockets.tcp.close(socketInfo.socketId, function() {
        chrome.sockets.tcp.getInfo(socketInfo.socketId, function(info) {
          chrome.test.assertEq(undefined, info);
          chrome.test.succeed();
        });
      });
    }

    chrome.test.assertTrue(socketInfo.socketId > 0);

    // Obtaining socket information before a connect() call should be safe, but
    // return empty values.
    chrome.sockets.tcp.getInfo(socketInfo.socketId, onGetInfo);
  }

  chrome.sockets.tcp.create({}, onCreate);
};

var testSending = function() {
  dataRead = "";
  echoDataSent = false;
  waitCount = 0;

  setTimeout(waitForBlockingOperation, 1000);

  createSocket();

  function createSocket() {
    chrome.sockets.tcp.create({
      "name": "test",
      "persistent": true,
      "bufferSize": 104
    }, onCreateComplete);
  }

  function onCreateComplete(socketInfo) {
    console.log("onCreateComplete");
    tcp_socketId = socketInfo.socketId;
    chrome.test.assertTrue(tcp_socketId > 0, "failed to create socket");

    console.log("add event listeners");
    receive_dispatcher[tcp_socketId] = {
      onReceive: onSocketReceive,
      onReceiveError: onSocketReceiveError
    };

    chrome.sockets.tcp.getInfo(tcp_socketId, onGetInfoAfterCreateComplete);
  }

  function onGetInfoAfterCreateComplete(result) {
    console.log("onGetInfoAfterCreateComplete");
    chrome.test.assertTrue(!result.localAddress,
                           "Socket should not have local address");
    chrome.test.assertTrue(!result.localPort,
                           "Socket should not have local port");
    chrome.test.assertTrue(!result.peerAddress,
                           "Socket should not have peer address");
    chrome.test.assertTrue(!result.peerPort,
                           "Socket should not have peer port");
    chrome.test.assertFalse(result.connected, "Socket should not be connected");

    chrome.test.assertEq("test", result.name, "Socket name did not persist");
    chrome.test.assertTrue(result.persistent,
                           "Socket should be persistent");
    chrome.test.assertEq(104, result.bufferSize, "Buffer size did not persist");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");

    chrome.sockets.tcp.update(tcp_socketId, {
      "name": "test2",
      "persistent": false,
      bufferSize: 2048
    }, onUpdateComplete);
  }

  function onUpdateComplete() {
    console.log("onUpdateComplete");
    chrome.sockets.tcp.getInfo(tcp_socketId, onGetInfoAfterUpdateComplete);
  }

  function onGetInfoAfterUpdateComplete(result) {
    console.log("onGetInfoAfterUpdateComplete");
    chrome.test.assertTrue(!result.localAddress,
                           "Socket should not have local address");
    chrome.test.assertTrue(!result.localPort,
                           "Socket should not have local port");
    chrome.test.assertTrue(!result.peerAddress,
                           "Socket should not have peer address");
    chrome.test.assertTrue(!result.peerPort,
                           "Socket should not have peer port");
    chrome.test.assertFalse(result.connected, "Socket should not be connected");

    chrome.test.assertEq("test2", result.name, "Socket name did not persist");
    chrome.test.assertFalse(result.persistent,
                           "Socket should not be persistent");
    chrome.test.assertEq(2048, result.bufferSize,
                         "Buffer size did not persist");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");

    chrome.sockets.tcp.connect(tcp_socketId, tcp_address, tcp_port,
                               onConnectComplete);
  }

  function onConnectComplete(result) {
    console.log("onConnectComplete");
    chrome.test.assertEq(0, result,
                         "Connect failed with error " + result);

    chrome.sockets.tcp.getInfo(tcp_socketId, onGetInfoAfterConnectComplete);
  }

  function onGetInfoAfterConnectComplete(result) {
    console.log("onGetInfoAfterConnectComplete");
    chrome.test.assertTrue(!!result.localAddress,
                           "Bound socket should always have local address");
    chrome.test.assertTrue(!!result.localPort,
                           "Bound socket should always have local port");

    // NOTE: We're always called with 'localhost', but getInfo will only return
    // IPs, not names.
    chrome.test.assertEq(result.peerAddress, "127.0.0.1",
                         "Peer addresss should be the listen server");
    chrome.test.assertEq(result.peerPort, tcp_port,
                         "Peer port should be the listen server");
    chrome.test.assertTrue(result.connected, "Socket should be connected");

    chrome.sockets.tcp.setPaused(tcp_socketId, true, onSetPausedComplete);
  }

  function onSetPausedComplete() {
    console.log("onSetPausedComplete");
    chrome.sockets.tcp.getInfo(tcp_socketId, onGetInfoAfterSetPausedComplete);
  }

  function onGetInfoAfterSetPausedComplete(result) {
    console.log("onGetInfoAfterSetPausedComplete");
    chrome.test.assertTrue(result.paused, "Socket should be paused");
    chrome.sockets.tcp.setPaused(tcp_socketId, false, onUnpauseComplete);
  }

  function onUnpauseComplete() {
    console.log("onUnpauseComplete");
    chrome.sockets.tcp.getInfo(tcp_socketId, onGetInfoAfterUnpauseComplete);
  }

  function onGetInfoAfterUnpauseComplete(result) {
    console.log("onGetInfoAfterUnpauseComplete");
    chrome.test.assertFalse(result.paused, "Socket should not be paused");
    chrome.sockets.tcp.setNoDelay(tcp_socketId, true, onSetNoDelayComplete);
  }

  function onSetNoDelayComplete(result) {
    console.log("onSetNoDelayComplete");
    if (result != 0) {
      chrome.test.fail("setNoDelay failed for TCP: " +
          "result=" + result + ", " +
          "lastError=" + chrome.runtime.lastError.message);
    }
    chrome.sockets.tcp.setKeepAlive(
        tcp_socketId, true, 1000, onSetKeepAliveComplete);
  }

  function onSetKeepAliveComplete(result) {
    console.log("onSetKeepAliveComplete");
    if (result != 0) {
      chrome.test.fail("setKeepAlive failed for TCP: " +
          "result=" + result + ", " +
          "lastError=" + chrome.runtime.lastError.message);
    }

    string2ArrayBuffer(httpPost, function(arrayBuffer) {
      echoDataSent = true;
      chrome.sockets.tcp.send(tcp_socketId, arrayBuffer, onSendComplete);
    });
  }

  function onSendComplete(sendInfo) {
    console.log("onSendComplete: " + sendInfo.bytesSent + " bytes.");
    chrome.test.assertEq(0, sendInfo.resultCode, "Send failed.");
    chrome.test.assertTrue(sendInfo.bytesSent > 0,
        "Send didn't write bytes.");
    bytesSent += sendInfo.bytesSent;
  }

  function onSocketReceive(receiveInfo) {
    console.log("onSocketReceive");
    chrome.test.assertEq(tcp_socketId, receiveInfo.socketId);
    arrayBuffer2String(receiveInfo.data, function(s) {
      // We may receive the response in multiple chunks. Keep appending the
      // response to dataAsString until we reach minExpectedResponseLength.
      dataAsString += s;
      if (dataAsString.length >= minExpectedResponseLength) {
        var match = !!dataAsString.match(expectedResponsePattern);
        chrome.test.assertTrue(match, "Received data does not match.");
        console.log("echo data received, closing socket");
        chrome.sockets.tcp.close(tcp_socketId, onCloseComplete);
      }
    });
  }

  function onSocketReceiveError(receiveErrorInfo) {
    // Note: Once we have sent the echo message, the server sends back
    // the echo response and closes the connection right away. This means
    // we get a "connection closed" error very quickly after sending our
    // message. This is why we ignore errors from that point on.
    if (echoDataSent)
      return;

    console.log("onSocketReceiveError");
    chrome.test.fail("Receive error on socket " + receiveErrorInfo.socketId +
      ": result code=" + receiveErrorInfo.resultCode);
  }

  function onCloseComplete() {
    console.log("onCloseComplete");
    chrome.test.succeed();
  }

};  // testSending()

var testSecure = function () {
  var request_sent = false;
  dataAsString = "";
  setTimeout(waitForBlockingOperation, 1000);

  // Run the test a few times. First with misuse_testing=MISUSE_NONE,
  // to test that the API works correctly when used properly. Then
  // with different values of misuse_mode, test that the API does
  // not malfunction when used improperly. Upon success, each misuse
  // must close the socket and call onCloseComplete().
  var MISUSE_NONE = 0;
  var MISUSE_SECURE_PENDING_READ = 1;
  var MISUSE_LAST = MISUSE_SECURE_PENDING_READ;
  var misuse_mode = MISUSE_NONE;

  chrome.sockets.tcp.create({}, onCreateComplete);

  function onCreateComplete(socketInfo) {
    https_socketId = socketInfo.socketId;
    receive_dispatcher[https_socketId] = {
      onReceive: onSocketReceive,
      onReceiveError: onSocketReceiveError
    };

    chrome.test.assertTrue(https_socketId > 0, "failed to create socket");
    if (misuse_mode == MISUSE_SECURE_PENDING_READ) {
      // Don't pause the socket. This will let the sockets runtime
      // keep a pending read on it.
      console.log("HTTPS onCreateComplete: in MISUSE_SECURE_PENDING_READ " +
                  "mode, skipping setPaused(false).");
      onPausedComplete();
    } else {
      chrome.sockets.tcp.setPaused(https_socketId, true, onPausedComplete);
    }
  }

  function onPausedComplete() {
    console.log("HTTPS onPausedComplete. Connecting to " + https_address + ":" +
        https_port);
    chrome.sockets.tcp.connect(https_socketId, https_address, https_port,
                               onConnectComplete);
  }

  function onConnectComplete(result) {
    console.log("HTTPS onConnectComplete");
    chrome.test.assertEq(0, result,
                         "Connect failed with error " + result);
    chrome.sockets.tcp.secure(https_socketId, null, onSecureComplete);
  }

  function onSecureComplete(result) {
    console.log("HTTPS onSecureComplete(" + result + ")");
    if (misuse_mode == MISUSE_SECURE_PENDING_READ) {
      chrome.test.assertFalse(result == 0,
                              "Secure should have failed when a read " +
                              "was pending (" + result + ")");
      chrome.sockets.tcp.close(https_socketId, onCloseComplete);
    } else {
      chrome.test.assertEq(0, result,
                           "Secure failed with error " + result);
      chrome.sockets.tcp.setPaused(https_socketId, false, onUnpauseComplete);
    }
  }

  function onUnpauseComplete() {
    console.log("HTTPS onUnpauseComplete");
    string2ArrayBuffer(httpPost, function(arrayBuffer) {
      request_sent = true;
      chrome.sockets.tcp.send(https_socketId, arrayBuffer, onSendComplete);
    });
  }

  function onSendComplete(sendInfo) {
    console.log("HTTPS onSendComplete: " + sendInfo.bytesSent + " bytes.");
    chrome.test.assertEq(0, sendInfo.resultCode, "Send failed.");
    chrome.test.assertTrue(sendInfo.bytesSent > 0,
        "Send didn't write bytes.");
    bytesSent += sendInfo.bytesSent;
  }

  function onSocketReceive(receiveInfo) {
    console.log("HTTPS onSocketReceive");
    chrome.test.assertEq(https_socketId, receiveInfo.socketId);
    arrayBuffer2String(receiveInfo.data, function(s) {
      // we will get more data than we care about. We only care about the
      // first segment of data (the HTTP 200 code). Ignore the rest, which
      // won't match the pattern.
      dataAsString += s;
      if (dataAsString.length >= minExpectedResponseLength) {
        console.log("HTTPS receive: got " + s);
        var match = !!dataAsString.match(expectedResponsePattern);
        chrome.test.assertTrue(match, "Received data does not match.");
        console.log("HTTPS response received, closing socket.");
        chrome.sockets.tcp.close(https_socketId, onCloseComplete);
      }
    });
  }

  function onSocketReceiveError(receiveErrorInfo) {
    console.log("HTTPS onSocketReceiveError");
    if (request_sent) {
      return;
    }
    chrome.test.fail("Receive error on socket " + receiveErrorInfo.socketId +
      ": result code=" + receiveErrorInfo.resultCode);
  }

  function onCloseComplete() {
    console.log("HTTPS Test Succeeded");
    if (misuse_mode == MISUSE_LAST) {
        // The test has run in all misuse modes.
        chrome.test.succeed();
    } else {
        // Run the test again in the next misuse mode.
        misuse_mode += 1;
        chrome.sockets.tcp.create({}, onCreateComplete);
    }
  }
};  // testSecure()

var testSendWriteQuota = function() {
  createSocket();

  function createSocket() {
    console.log("createSocket");
    chrome.sockets.tcp.create({
      "name": "test",
      "persistent": true,
      "bufferSize": 104
    }, onCreateComplete);
  }

  function onCreateComplete(socketInfo) {
    console.log("onCreateComplete");
    tcp_socketId = socketInfo.socketId;
    chrome.test.assertTrue(tcp_socketId > 0, "failed to create socket");

    string2ArrayBuffer(httpPost, function(arrayBuffer) {
      chrome.sockets.tcp.send(tcp_socketId, arrayBuffer, onSendComplete);
    });
  }

  function onSendComplete(sendInfo) {
    console.log("onSendComplete: ", sendInfo,
                ', lastError=', chrome.runtime.lastError);
    if (chrome.runtime.lastError &&
        chrome.runtime.lastError.message == "Exceeded write quota.") {
      chrome.test.succeed();
      return;
    }

    chrome.test.fail('Write quota not enforced');
  }
};  // testSendWriteQuota

function waitForBlockingOperation() {
  if (++waitCount < 10) {
    setTimeout(waitForBlockingOperation, 1000);
  } else {
    // We weren't able to succeed in the given time.
    chrome.test.fail("Operations didn't complete after " + waitCount + " " +
                     "seconds. Response so far was <" + dataAsString + ">.");
  }
}

var onMessageReply = function(message) {
  var components = message.split(',');
  var tests = [];
  for (var i = 0; i < components.length; ++i) {
    var parts = components[i].split(":");
    var test_type = parts[0];
    if (test_type == 'tcp') {
      tcp_address = parts[1];
      tcp_port = parseInt(parts[2]);
      console.log("Running tests for TCP, server " +
          tcp_address + ":" + tcp_port);
      tests = tests.concat([testSocketCreation, testSending]);
    } else if (test_type == 'https') {
      https_address = parts[1];
      https_port = parseInt(parts[2]);
      console.log("Running tests for HTTPS, server " +
          https_address + ":" + https_port);
      tests = tests.concat([testSecure]);
    } else if (test_type == 'tcp_send_write_quota') {
      tcp_address = parts[1];
      tcp_port = parseInt(parts[2]);
      console.log("Running tests for TCP, server " +
          tcp_address + ":" + tcp_port);
      tests = tests.concat([testSendWriteQuota]);
    } else {
      chrome.test.fail("Invalid test type: " + test_type);
    }
  }
  // Setup the suite-wide event listeners.
  chrome.sockets.tcp.onReceive.addListener(dispatchSocketReceive);
  chrome.sockets.tcp.onReceiveError.addListener(dispatchSocketReceiveError);

  chrome.test.runTests(tests);
};

// Find out which protocol we're supposed to test, and which echo server we
// should be using, then kick off the tests.
chrome.test.sendMessage("info_please", onMessageReply);
