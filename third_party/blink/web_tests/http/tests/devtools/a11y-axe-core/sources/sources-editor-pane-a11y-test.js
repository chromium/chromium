// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the editor pane in sources panel using the axe-core linter.');

  // axe-core issue #1444 -- role="tree" requires children with role="treeitem",
  // but it is reasonable to have trees with no leaves.
  // Ignore 'aria-required-children' rule for tablist because it doesn't accommodate empty tablist.
  const NO_REQUIRED_CHILDREN_RULESET = {
    'aria-required-children': {
      enabled: false,
      selector: ':not(.tabbed-pane-header-tabs)'
    },
    'aria-allowed-attr': {
      enabled: false,
      selector: '.cm-content'
    }
  };


  await UI.ViewManager.ViewManager.instance().showView('sources');

  await setup();
  await runTest();

  TestRunner.completeTest();

  async function setup() {
    const projects = Workspace.Workspace.WorkspaceImpl.instance().projectsForType(Workspace.Workspace.projectTypes.FileSystem);
    const snippetsProject = projects.find(
      project => Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(project) === 'snippets');
    const uiSourceCode1 = await snippetsProject.createFile('');
    await Common.Revealer.reveal(uiSourceCode1);
    const uiSourceCode2 = await snippetsProject.createFile('');
    await Common.Revealer.reveal(uiSourceCode2);
  }

  async function runTest() {
    // Verify contents of the TabHeader to make sure files are open
    const tabbedPane = Sources.SourcesPanel.SourcesPanel.instance().sourcesView().editorContainer.tabbedPane;
    const tabs = tabbedPane.tabs;
    TestRunner.addResult('All tabs:');
    tabs.forEach(tab => TestRunner.addResult(tab.title));
    TestRunner.addResult('\n');

    await runA11yTest();
  }

  async function runA11yTest() {
    await UI.ViewManager.ViewManager.instance().showView('sources');
    const element = Sources.SourcesPanel.SourcesPanel.instance().sourcesView().contentElement;
    await AxeCoreTestRunner.runValidation(element, NO_REQUIRED_CHILDREN_RULESET);
  }
})();
