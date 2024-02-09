// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests that console preserves log on oopif navigation`);
  await TestRunner.showPanel('console');

  Common.Settings.settingForTest('preserve-console-log').set(false);
  await TestRunner.navigatePromise('resources/empty.html');
  await TestRunner.evaluateInPage(`console.log('Before navigation')`);
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/oopif/resources/empty.html');
  await TestRunner.evaluateInPage(`console.log('After navigation')`);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
