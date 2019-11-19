// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test user agent setting\n`);
  await TestRunner.loadModule('network_test_runner');

  var cases = [
    'Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Safari/537.36',
    'Mozilla/5.0 (Windows NT 10.0; WOW64; rv:46.0) Gecko/20100101 Firefox/46.0',
    'GoogleChrome/%s Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Safari/537.36',
    'Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s Safari/537.36 Edg/%s',
  ];

  for (var i = 0; i < cases.length; i++) {
    var result = SDK.MultitargetNetworkManager.patchUserAgentWithChromeVersion(cases[i]);
    TestRunner.addResult(result);
  }

  TestRunner.addResult('\nManually setting custom user agent');
  SDK.multitargetNetworkManager.setCustomUserAgentOverride('foobar with %s inside');

  TestRunner.evaluateInPage('navigator.userAgent', step2);

  function step2(result) {
    TestRunner.addResult(result);
    TestRunner.completeTest();
  }
})();
