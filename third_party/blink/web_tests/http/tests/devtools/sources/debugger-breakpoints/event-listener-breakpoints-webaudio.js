// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests event listener breakpoints for WebAudio.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      let audioContext;
  `);

  SourcesTestRunner.startDebuggerTest(audioContextCreated);

  function audioContextCreated() {
    SourcesTestRunner.setEventListenerBreakpoint(
        'instrumentation:audioContextCreated', true);
    SourcesTestRunner.waitUntilPaused(() => {
      TestRunner.addResult('Successfully paused after AudioContext construction.');
      SourcesTestRunner.resumeExecution(audioContextSuspended);
    });
    TestRunner.evaluateInPageWithTimeout('audioContext = new AudioContext()');
  }

  function audioContextSuspended() {
    SourcesTestRunner.setEventListenerBreakpoint(
        'instrumentation:audioContextSuspended', true);
    SourcesTestRunner.waitUntilPaused(() => {
      TestRunner.addResult('Successfully paused after AudioContext suspension.');
      SourcesTestRunner.resumeExecution(audioContextResumed);
    });
    TestRunner.evaluateInPageWithTimeout('audioContext.suspend()');
  }

  function audioContextResumed() {
    SourcesTestRunner.setEventListenerBreakpoint(
        'instrumentation:audioContextResumed', true);
    SourcesTestRunner.waitUntilPaused(() => {
      TestRunner.addResult('Successfully paused after AudioContext resumption.');
      SourcesTestRunner.resumeExecution(audioContextClosed);
    });
    TestRunner.evaluateInPageWithTimeout('audioContext.resume()');
  }

  function audioContextClosed() {
    SourcesTestRunner.setEventListenerBreakpoint(
        'instrumentation:audioContextClosed', true);
    SourcesTestRunner.waitUntilPaused(() => {
      TestRunner.addResult('Successfully paused after AudioContext closure.');
      SourcesTestRunner.resumeExecution(finish);
    });
    TestRunner.evaluateInPageWithTimeout('audioContext.close()');
  }

  function finish() {
    SourcesTestRunner.completeDebuggerTest();
  }

})();
