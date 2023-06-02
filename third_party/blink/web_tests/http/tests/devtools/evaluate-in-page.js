// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`This tests that layout test can evaluate scripts in the inspected page.\n`);
  await TestRunner.evaluateInPagePromise(`
      function sum(a, b)
      {
          return a + b;
      }
  `);

  function callback(result) {
    TestRunner.addResult('2 + 2 = ' + result);
    TestRunner.completeTest();
  }
  TestRunner.evaluateInPage('sum(2, 2)', callback);
})();
