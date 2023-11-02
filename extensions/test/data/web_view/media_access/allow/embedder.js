// Copyright 2013 The Chromium Authors
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
  window.console.warn('test failure, reason: ' + msg);
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
embedder.setUpLoadStop_ = function(webview, testName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    // This will make the guest start issueing media request. We do not
    // worry about the Javascript outcome. MockWebContestsDelegate in
    // WebViewTest will take care of that.
    webview.contentWindow.postMessage(
        JSON.stringify(['check-media-permission', '' + testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.assertCorrectMediaEvent_ = function(e) {
  if (e.permission != 'media') {
    embedder.failTest('wrong permission: ' + e.permission);
    return false;
  }
  if (!e.url || !e.request.url) {
    embedder.failTest('No url property in event');
    return false;
  }
  if (e.url.indexOf(embedder.baseGuestURL)) {
    embedder.failTest('Wrong url: ' + e.url +
        ', expected url to start with ' + embedder.baseGuestURL);
    return false;
  }
  return true;
};

// Each test loads a guest which requests media access.
// All tests listed in this file expect the guest's request to be allowed.
//
// Once the guest is allowed media access, the MockWebContestsDelegate catches
// the fact and the test succeeds.
//
// Note that this is a manually run test, not using chrome.test.runTests.
// This is because we want to wait for MockWebContestsDelegate to catch media
// access request and not actually issue the media request.

embedder.tests.testAllow = function() {
  var webview = embedder.setUpGuest_();
  if (!webview) {
    return;
  }

  var onPermissionRequest = function(e) {
    if (!embedder.assertCorrectMediaEvent_(e)) {
      return;
    }
    e.request.allow();
    embedder.maybePassTest();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test1');
};

embedder.tests.testAllowAndThenDeny = function() {
  var webview = embedder.setUpGuest_();
  if (!webview) {
    return;
  }

  var calledAllow = false;
  var callCount = 0;
  var exceptionCount = 0;
  var checkAndCall = function(e) {
    if (!embedder.assertCorrectMediaEvent_(e)) {
      return;
    }

    if (!calledAllow) {
      e.request.allow();
      calledAllow = true;
      ++callCount
    } else {
      try {
        e.request.deny();
      } catch (exception) {
        ++exceptionCount;
      }
    }

    if (callCount == 1 && exceptionCount == 1) {
      embedder.maybePassTest();
    }
  };

  var onPermissionRequest1 = function(e) {
    checkAndCall(e);
  };
  var onPermissionRequest2 = function(e) {
    checkAndCall(e);
  };
  webview.addEventListener('permissionrequest', onPermissionRequest1);
  webview.addEventListener('permissionrequest', onPermissionRequest2);

  embedder.setUpLoadStop_(webview, 'test1');
};

embedder.tests.testAllowAsync = function() {
  var webview = embedder.setUpGuest_();
  if (!webview) {
    return;
  }

  var onPermissionRequest = function(e) {
    if (!embedder.assertCorrectMediaEvent_(e)) {
      return;
    }

    e.preventDefault();
    // Decide asynchronously.
    window.setTimeout(function() {
      e.request.allow();
      embedder.maybePassTest();
    }, 0);
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test1');
};

embedder.tests.testAllowTwice = function() {
  var webview = embedder.setUpGuest_();
  if (!webview) {
    return;
  }

  var calledAllow = false;
  var callbackCount = 0;
  var exceptionCount = 0;
  var checkAndCall = function(e) {
    if (!embedder.assertCorrectMediaEvent_(e)) {
      return;
    }

    ++callbackCount;
    try {
      e.request.allow();
    } catch (exception) {
      ++exceptionCount;
    }

    if (callbackCount == 2) {
      if (exceptionCount == 1) {
        embedder.maybePassTest();
      } else {
        embedder.failTest('Expected exceptionCount 1, but got ' +
            exceptionCount);
      }
    }
  };

  var onPermissionRequest1 = function(e) {
    checkAndCall(e);
  };
  var onPermissionRequest2 = function(e) {
    checkAndCall(e);
  };
  webview.addEventListener('permissionrequest', onPermissionRequest1);
  webview.addEventListener('permissionrequest', onPermissionRequest2);

  embedder.setUpLoadStop_(webview, 'test1');
};

embedder.tests.list = {
  'testAllow': embedder.tests.testAllow,
  'testAllowAndThenDeny': embedder.tests.testAllowAndThenDeny,
  'testAllowAsync': embedder.tests.testAllowAsync,
  'testAllowTwice': embedder.tests.testAllowTwice
};

// Entry point for test, called by WebViewTest.
function runTest(testName) {
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
    embedder.guestURL = embedder.baseGuestURL + '/media_access_guest.html';
    chrome.test.log('Guest url is: ' + embedder.guestURL);

    var testFunction = embedder.tests.list[testName];
    if (!testFunction) {
      embedder.failTest('No such test: ' + testName);
      return;
    }
    testFunction();
  });
}

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
