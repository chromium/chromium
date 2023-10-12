// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      'Checks that we show correct location for script evaluated twice.\n');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPageAnonymously('console.log(1);//# sourceURL=a.js');
  await TestRunner.evaluateInPageAnonymously('console.log(2);//# sourceURL=a.js');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
