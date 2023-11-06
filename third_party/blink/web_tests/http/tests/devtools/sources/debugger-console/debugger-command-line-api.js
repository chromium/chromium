// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Elements from 'devtools/panels/elements/elements.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that inspect() command line api works while on breakpoint.\n`);
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

  TestRunner.addSniffer(SDK.RuntimeModel.RuntimeModel.prototype, 'inspectRequested', inspect);
  const originalReveal = Common.Revealer.reveal;
  Common.Revealer.setRevealForTest((node) => {
    if (!(node instanceof SDK.RemoteObject.RemoteObject)) {
      return Promise.resolve();
    }
    return originalReveal(node).then(updateFocusedNode);
  });

  function updateFocusedNode() {
    TestRunner.addResult('Selected node id: \'' + Elements.ElementsPanel.ElementsPanel.instance().selectedDOMNode().getAttribute('id') + '\'.');
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
