// Copyright 2021 Igalia S.A. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult('Tests that debugger statement stops execution from eval on detached iframes\n');
  await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  let script = `
    i = document.createElement('iframe');
    document.body.appendChild(i);
    w = i.contentWindow;
    w.eval('window');
    i.remove();
    w.eval('debugger;window');
  `;

  ProtocolClient.test.suppressRequestErrors = true;
  SourcesTestRunner.runDebuggerTestSuite([function (next) {
    SourcesTestRunner.waitUntilPaused(paused);
    TestRunner.evaluateInPage(script);

    async function paused(callFrames) {
      await SourcesTestRunner.captureStackTrace(callFrames);
      SourcesTestRunner.resumeExecution(next);
    }
  }]);
})();
