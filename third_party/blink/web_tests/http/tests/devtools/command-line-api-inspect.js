// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect() command line api works.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadHTML(`
      <p id="p1">
      </p>
    `);

  TestRunner.addSniffer(SDK.RuntimeModel.prototype, 'inspectRequested', sniffInspect, true);

  function sniffInspect(objectId, hints) {
    TestRunner.addResult('WebInspector.inspect called with: ' + objectId.description);
    TestRunner.addResult('WebInspector.inspect\'s hints are: ' + JSON.stringify(Object.keys(hints)));
  }

  function evalAndDump(expression, next) {
    TestRunner.addResult('\n');
    ConsoleTestRunner.evaluateInConsole(expression, dumpCallback);
    function dumpCallback(text) {
      TestRunner.addResult(expression + ' = ' + text);
      if (next)
        next();
    }
  }

  TestRunner.runTestSuite([function testRevealElement(next) {
    const originalReveal = Common.Revealer.reveal;
    Common.Revealer.setRevealForTest((node) => {
      if (!(node instanceof SDK.RemoteObject)) {
        return Promise.resolve();
      }
      return originalReveal(node).then(step3);
    });
    evalAndDump('inspect($(\'#p1\'))');

    function step3() {
      TestRunner.addResult('Selected node id: \'' + UI.panels.elements.selectedDOMNode().getAttribute('id') + '\'.');
      next();
    }
  }]);
})();
