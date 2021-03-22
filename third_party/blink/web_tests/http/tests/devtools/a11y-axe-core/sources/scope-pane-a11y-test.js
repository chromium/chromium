// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await UI.viewManager.showView('sources.scopeChain');

  TestRunner.addResult('Testing accessibility in the scope pane.\n');
  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.evaluateInPagePromise(`
  function makeClosure() {
    let x = 5;
    function logX() {
      console.log(x);
      debugger;
    }
    return logX;
  }
  makeClosure()();
  `);
  await SourcesTestRunner.waitUntilPausedPromise();

  await TestRunner.addSnifferPromise(Sources.ScopeChainSidebarPane.prototype, '_sidebarPaneUpdatedForTest');
  const scopePane = Sources.ScopeChainSidebarPane.instance();
  await TestRunner.addSnifferPromise(ObjectUI.ObjectPropertyTreeElement, 'populateWithProperties');
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  TestRunner.addResult('Expanding the makeClosure closure.');
  scopePane._treeOutline._rootElement.childAt(1).expand();
  await TestRunner.addSnifferPromise(ObjectUI.ObjectPropertyTreeElement, 'populateWithProperties');
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  SourcesTestRunner.completeDebuggerTest();
})();
