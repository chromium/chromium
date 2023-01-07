// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
// These variables will be filled in chrome.test.getConfig() below.
embedder.baseGuestURL = '';
embedder.guestURL = '';

// Sends a message to WebViewTest denoting it is done and test
// has failed.
embedder.failTest = function(msg) {
  window.console.log('test failure, reason: ' + msg);
  chrome.test.sendMessage('TEST_FAILED');
};

// Sends a message to WebViewTest denoting it is done and test
// has succeeded.
embedder.maybePassTest = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      '></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    embedder.failTest('No <webview> element created');
    return null;
  }
  return webview;
};

/** @private */
embedder.setUpLoadStop_ = function(webview) {
  var onWebViewLoadStop = function(e) {
    window.console.log('onWebViewLoadStop');

    // Send post message to <webview> when it's ready to receive them.
    // This will make the guest start issueing media request. We do not
    // worry about the Javascript outcome. MockWebContestsDelegate in
    // WebViewTest will take care of that.
    webview.contentWindow.postMessage(
        JSON.stringify(['get-sources-permission']), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

// The test loads a guest which requests media sources, which will in turn check
// for media access permission.
//
// Note that this is a manually run test, not using chrome.test.runTests.
// This is because we want to wait for MockWebContestsDelegate to catch the
// media access check and not actually do a check.

// Entry point for test, called by WebViewTest.
function runTest(testName) {
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
    embedder.guestURL = embedder.baseGuestURL + '/media_check_guest.html';
    chrome.test.log('Guest url is: ' + embedder.guestURL);

    var webview = embedder.setUpGuest_();
    if (!webview) {
      return;
    }

    embedder.setUpLoadStop_(webview);

    webview.addEventListener('consolemessage', function(e) {
      window.console.log(e.message);
    });

    window.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      if (data[0] == 'got-sources') {
        embedder.maybePassTest();
      } else {
        window.console.log('Unexpected message: ' + e.message);
      }
    });
  });
}

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
