// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as BindingsModule from 'devtools/models/bindings/bindings.js';

(async function() {
  TestRunner.addResult(`Tests stopping in debugger in the worker with source mapping.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function installWorker()
      {
          new Worker("resources/worker-compiled.js");
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    TestRunner.evaluateInPage('installWorker()');
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.addSniffer(BindingsModule.CompilerScriptMapping.CompilerScriptMapping.prototype, 'sourceMapAttachedForTest', sourceMapLoaded);
  }

  var callFrames;
  var callbacksLeft = 2;

  function paused(callFramesParam) {
    callFrames = callFramesParam;
    maybeFinishTest();
  }

  function sourceMapLoaded() {
    maybeFinishTest();
  }

  async function maybeFinishTest() {
    if (--callbacksLeft)
      return;
    await SourcesTestRunner.captureStackTrace(callFrames);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
