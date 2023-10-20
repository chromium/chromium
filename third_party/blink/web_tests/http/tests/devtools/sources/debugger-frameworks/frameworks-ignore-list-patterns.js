// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Bindings from 'devtools/models/bindings/bindings.js';

(async function() {
  TestRunner.addResult(`Tests framework ignore list patterns for various URLs.\n`);
  await TestRunner.showPanel('sources');

  var testCases = [
    'http://www.example.com/foo/jquery-1.7-min.js',
    '/jquery\\-1\\.7\\-min\\.js$',
    'https://www.example.com/jquery.js?version=1.7',
    '/jquery\\.js\\b',
    'http://www.google.com/jsapi',
    '/jsapi$',
    'https://www.google.com/jsapi/',
    '/jsapi/$',
    'http://www.google.com',
    'www\\.google\\.com$',
    'http://www.google.com/',
    'www\\.google\\.com\\b',
    'data:text/html,foo',
    '',
    'about:blank',
    '',
    '',
    '^$',
    'chrome-extension://extensionName/main.js',
    '^chrome-extension://extensionName\\b.*/main\\.js$',
    'extensions::unload_events',
    '^extensions::unload_events$',
  ];

  for (var i = 0; i < testCases.length; i += 2) {
    var url = testCases[i];
    TestRunner.addResult('Testing "' + url + '"');
    var regexValue = Bindings.IgnoreListManager.IgnoreListManager.instance().urlToRegExpString(url);
    TestRunner.assertEquals(testCases[i + 1], regexValue);
    if (!regexValue)
      continue;
    var regex = new RegExp(regexValue);
    TestRunner.assertTrue(regex.test(url), 'FAIL: Generated RegExp does not match the URL');
  }

  TestRunner.addResult('\nPASS');
  TestRunner.completeTest();
})();
