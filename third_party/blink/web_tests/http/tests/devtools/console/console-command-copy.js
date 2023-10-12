// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console's copy command is copying into front-end buffer.\n`);

  await TestRunner.showPanel('console');

  var results = [];
  var testCases = [
    "copy('qwerty')",
    "copy(document.querySelector('p'))",
    "copy({foo:'bar'})",
    'var a = {}; a.b = a; copy(a)',
    'copy(NaN)',
    'copy(Infinity)',
    'copy(null)',
    'copy(undefined)',
    'copy(1)',
    'copy(true)',
    'copy(false)',
    'copy(null)'
  ];

  function copyText(text) {
    results.push(text);
    if (results.length === testCases.length) {
      results.sort();
      for (var result of results) TestRunner.addResult('InspectorFrontendHost.copyText: ' + result);
      TestRunner.completeTest();
    }
  }

  InspectorFrontendHost.copyText = copyText;
  for (var i = 0; i < testCases.length; ++i) TestRunner.RuntimeAgent.evaluate(testCases[i], '', true);
})();
