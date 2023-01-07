// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var util = {};
var embedder = {};

window.runTest = function(testName, appToEmbed) {
  if (!embedder.test.testList[testName]) {
    window.console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName](appToEmbed);
};

var LOG = function(msg) {
  window.console.log(msg);
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('Assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('Assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('Assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

// Tests begin.
function testAppViewGoodDataShouldSucceed(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  LOG('Attempting to connect to app with good params.');
  // Step 2: Attempt to connect to an app with good params.
  appview.connect(appToEmbed, {'foo': 'bleep'}, function(success) {
    // Make sure we don't fail.
    if (!success) {
      LOG('FAILED TO CONNECT.');
      embedder.test.fail();
      return;
    }
    LOG('Connected.');
    embedder.test.succeed();
  });
};

function testAppViewMediaRequest(appToEmbed) {
  var appview = new AppView();
  window.console.log('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  window.console.log('Attempting to connect to app.');
  appview.connect(appToEmbed, {}, function(success) {
    // Make sure we don't fail.
    if (!success) {
      window.console.log('Failed to connect.');
      embedder.test.fail();
      return;
    }
    window.console.log('Connected.');
    embedder.test.succeed();
  });
};

function testAppViewRefusedDataShouldFail(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  LOG('Attempting to connect to app with refused params.');
  appview.connect(appToEmbed, {'foo': 'bar'}, function(success) {
    // Make sure we fail.
    if (success) {
      LOG('UNEXPECTED CONNECTION.');
      embedder.test.fail();
      return;
    }
    LOG('Failed to connect.');
    embedder.test.succeed();
  });
};

function testAppViewWithUndefinedDataShouldSucceed(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  // Step 1: Attempt to connect to a non-existent app (abc123).
  LOG('Attempting to connect to non-existent app.');
  appview.connect('abc123', undefined, function(success) {
    // Make sure we fail.
    if (success) {
      LOG('UNEXPECTED CONNECTION.');
      embedder.test.fail();
      return;
    }
    LOG('failed to connect to non-existent app.');
    LOG('attempting to connect to known app.');
    // Step 2: Attempt to connect to an app we know exists.
    appview.connect(appToEmbed, undefined, function(success) {
      // Make sure we don't fail.
      if (!success) {
        LOG('FAILED TO CONNECT.');
        embedder.test.fail();
        return;
      }
      LOG('Connected.');
      embedder.test.succeed();
    });
  });
};

function testAppViewNoEmbedRequestListener(appToEmbed) {
  var appview = new AppView();
  LOG('appToEmbed  ' + appToEmbed);
  document.body.appendChild(appview);
  LOG('Attempting to connect to app that does not listen for embed requests.');
  appview.connect(appToEmbed, null, (success) => {
    if (success) {
      LOG('Should not have connected.');
      embedder.test.fail();
    } else {
      LOG('Connection was correctly rejected.');
      embedder.test.succeed();
    }
  });
};

embedder.test.testList = {
  'testAppViewGoodDataShouldSucceed': testAppViewGoodDataShouldSucceed,
  'testAppViewMediaRequest': testAppViewMediaRequest,
  'testAppViewRefusedDataShouldFail': testAppViewRefusedDataShouldFail,
  'testAppViewWithUndefinedDataShouldSucceed':
      testAppViewWithUndefinedDataShouldSucceed,
  'testAppViewNoEmbedRequestListener': testAppViewNoEmbedRequestListener
};

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
