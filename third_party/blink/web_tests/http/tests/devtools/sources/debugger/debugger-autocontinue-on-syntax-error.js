// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that debugger won't stop on syntax errors even if "pause on uncaught exceptions" is on.\n`);
  await TestRunner.showPanel('sources');

  SourcesTestRunner.startDebuggerTest(step1);

  async function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    await TestRunner.addIframe('resources/syntax-error.html');
    await ConsoleTestRunner.dumpConsoleMessages();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
