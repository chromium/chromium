// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_ID = 'bjafgdebaacbbbecmhlhpofkepfkgcpa';

// Call with |api| as either chrome.runtime or chrome.extension.
function connectExternalTest(api) {
  let port = api.connect(TEST_ID, {name: 'extern'});
  port.postMessage({testConnectExternal: true});
  port.onMessage.addListener(chrome.test.callbackPass(function(msg) {
    chrome.test.assertTrue(msg.success, 'Message failed.');
    chrome.test.assertEq(msg.senderId, location.host,
                         'Sender ID doesn\'t match.');
  }));
}

// Generates the list of test functions.
function generateTests() {
  let tests = [function connectExternal_runtime() {
    connectExternalTest(chrome.runtime);
  }];

  // In Chrome, also test the same functions on chrome.extension.
  if (chrome.extension) {
    tests.push(function connectExternal_extension() {
      connectExternalTest(chrome.extension);
    });
  }

  return tests;
}

chrome.test.runTests(generateTests());
