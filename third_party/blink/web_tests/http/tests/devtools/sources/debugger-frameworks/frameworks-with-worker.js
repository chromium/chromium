// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Main from 'devtools/entrypoints/main/main.js';

(async function() {
  TestRunner.addResult(`Tests that blackboxed script will be skipped while stepping on worker.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function installWorker()
      {
          new Worker("../resources/worker-source.js");
      }
  `);

  var frameworkRegexString = 'foo\\.js$';
  Main.MainImpl.MainImpl.universeForTest.settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.startDebuggerTest(step1, true);
  function step1() {
    var actions = ['StepOver', 'StepInto', 'Print'];
    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, () => SourcesTestRunner.completeDebuggerTest());
    TestRunner.evaluateInPage('installWorker()');
  }
})();
