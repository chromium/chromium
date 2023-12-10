// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that we skip all pauses during navigation`);
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise();
  await TestRunner.navigatePromise('resources/page-with-pagehide.html');
  TestRunner.addResult('Navigate page..');
  TestRunner.evaluateInPagePromise('window.location.href = window.location.href');
  TestRunner.addResult('Wait for ready message..');
  await ConsoleTestRunner.waitUntilMessageReceivedPromise();
  TestRunner.addResult('done!');

  await TestRunner.addIframe('http://127.0.0.1:8000/devtools/sources/debugger/resources/onunload.html', {
    name: 'myIFrame'
  });

  ConsoleTestRunner.changeExecutionContext('myIFrame');
  ConsoleTestRunner.evaluateInConsolePromise('window.location.href = window.location.href', true);
  await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.resumeExecution();

  SourcesTestRunner.completeDebuggerTest();
})();
