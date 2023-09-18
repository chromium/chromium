// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the Sources panel Navigator pane Snippets tab using axe-core.');

  // axe-core issue #1444 -- role="tree" requires children with role="treeitem",
  // but it is reasonable to have trees with no leaves.
  const NO_REQUIRED_CHILDREN_RULESET = {
    'aria-required-children': {
      enabled: false,
    },
  };

  await TestRunner.loadLegacyModule('snippets');

  await UI.viewManager.showView('sources');

  await setup();
  await testA11yForView(NO_REQUIRED_CHILDREN_RULESET);

  TestRunner.completeTest();

  async function setup() {
    // Add snippets
    await Snippets.ScriptSnippetFileSystem.findSnippetsProject().createFile('s1', null, '');
    await Snippets.ScriptSnippetFileSystem.findSnippetsProject().createFile('s2', null, '');
  }

  async function testA11yForView(ruleSet) {
    await UI.viewManager.showView('navigator-snippets');
    const sourcesNavigatorView = new SourcesModule.SourcesNavigator.SnippetsNavigatorView();

    sourcesNavigatorView.show(UI.inspectorView.element);
    SourcesTestRunner.dumpNavigatorView(sourcesNavigatorView);
    const element = UI.panels.sources.navigatorTabbedLocation.tabbedPane().element;
    await AxeCoreTestRunner.runValidation(element, ruleSet);
  }
})();
