// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect() command line api works while on breakpoint.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="p1">
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
      }
  `);

  TestRunner.addSniffer(SDK.RuntimeModel.prototype, 'inspectRequested', inspect);
  const originalReveal = Common.Revealer.reveal;
  Common.Revealer.setRevealForTest((node) => {
    if (!(node instanceof SDK.RemoteObject)) {
      return Promise.resolve();
    }
    return originalReveal(node).then(updateFocusedNode);
  });

  function updateFocusedNode() {
    TestRunner.addResult('Selected node id: \'' + UI.panels.elements.selectedDOMNode().getAttribute('id') + '\'.');
    SourcesTestRunner.completeDebuggerTest();
  }

  function inspect(objectId, hints) {
    TestRunner.addResult('WebInspector.inspect called with: ' + objectId.description);
    TestRunner.addResult('WebInspector.inspect\'s hints are: ' + JSON.stringify(Object.keys(hints)));
  }

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2(callFrames) {
    ConsoleTestRunner.evaluateInConsoleAndDump('inspect($(\'#p1\'))');
  }
})();
