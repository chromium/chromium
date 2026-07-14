// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as ObjectUI from 'devtools/ui/legacy/components/object_ui/object_ui.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  await TestRunner.showPanel('sources');
  await UI.ViewManager.ViewManager.instance().showView('sources.scope-chain');

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

  const nodesPromise = TestRunner.addSnifferPromise(ObjectUI.ObjectPropertiesSection.ObjectPropertyTreeElement, 'createNodes');
  await TestRunner.addSnifferPromise(Sources.ScopeChainSidebarPane.ScopeChainSidebarPane.prototype, 'sidebarPaneUpdatedForTest');
  await nodesPromise;
  const scopePane = Sources.ScopeChainSidebarPane.ScopeChainSidebarPane.instance();
  await new Promise(requestAnimationFrame);
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  TestRunner.addResult('Expanding the makeClosure closure.');
  const secondNodesPromise = TestRunner.addSnifferPromise(ObjectUI.ObjectPropertiesSection.ObjectPropertyTreeElement, 'createNodes');
  const tree = scopePane.contentElement.querySelector('devtools-tree');
  const listItems = tree.shadowRoot.querySelectorAll('.scope-chain-sidebar-pane-section');
  listItems[1].dispatchEvent(new CustomEvent('expand', {detail: {expanded: true}}));
  await secondNodesPromise;
  await new Promise(requestAnimationFrame);
  TestRunner.addResult(`Scope pane content: ${scopePane.contentElement.deepTextContent()}`);
  TestRunner.addResult(`Running the axe-core linter on the scope pane.`);
  await AxeCoreTestRunner.runValidation(scopePane.contentElement);

  SourcesTestRunner.completeDebuggerTest();
})();
