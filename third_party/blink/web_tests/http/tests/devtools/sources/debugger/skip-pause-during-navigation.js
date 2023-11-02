// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that we skip all pauses during navigation`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise();
  await TestRunner.navigatePromise('resources/page-with-unload.html');
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
