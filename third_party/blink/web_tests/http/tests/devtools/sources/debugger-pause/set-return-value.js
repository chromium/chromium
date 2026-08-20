// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConsoleTestRunner} from 'console_test_runner';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {SourcesTestRunner} from 'sources_test_runner';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult('Check that return value can be changed.');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
    function testFunction() {
      Promise.resolve(42).then(x => x).then(console.log);
    }
    //# sourceURL=test.js
  `);
  await SourcesTestRunner.startDebuggerTestPromise();
  await TestRunner.DebuggerAgent.invoke_setBreakpointByUrl(
      {lineNumber: 15, url: 'test.js', columnNumber: 37});
  let sidebarUpdated = TestRunner.addSnifferPromise(
        SourcesModule.ScopeChainSidebarPane.ScopeChainSidebarPane.prototype, 'sidebarPaneUpdatedForTest');
  await Promise.all([SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise(), sidebarUpdated]);
  await UI.Widget.Widget.allUpdatesComplete;
  let localScope = SourcesTestRunner.scopeChainSections()[0];

  TestRunner.addResult('Dump current');
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  await UI.Widget.Widget.allUpdatesComplete;
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Set return value to {a:1}');
  let returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement.applyExpression('{a:1}');
  await UI.Widget.Widget.allUpdatesComplete;
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  await UI.Widget.Widget.allUpdatesComplete;
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Try to remove return value');
  returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement.applyExpression('');
  await UI.Widget.Widget.allUpdatesComplete;
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  await UI.Widget.Widget.allUpdatesComplete;
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  TestRunner.addResult('Set return value to 239');
  returnValueElement = localScope.children().find(x => x.property.name === 'Return value');
  await returnValueElement.applyExpression('239');
  await UI.Widget.Widget.allUpdatesComplete;
  await new Promise(resolve => SourcesTestRunner.expandProperties([localScope, ['Return value']], resolve));
  await UI.Widget.Widget.allUpdatesComplete;
  SourcesTestRunner.dumpScopeVariablesSidebarPane();

  SourcesTestRunner.resumeExecution();
  await ConsoleTestRunner.waitUntilMessageReceivedPromise();
  TestRunner.addResult('Actual return value:');
  await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();

  SourcesTestRunner.completeDebuggerTest();
})();
