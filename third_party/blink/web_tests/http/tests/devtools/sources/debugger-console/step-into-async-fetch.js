// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function test() {
  TestRunner.addResult('Checks stepInto fetch');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  ConsoleTestRunner.evaluateInConsole(`
    debug(fetch);
    fetch("../debugger/resources/script1.js");
    //# sourceURL=test.js`);
  await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.stepIntoAsync();
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();