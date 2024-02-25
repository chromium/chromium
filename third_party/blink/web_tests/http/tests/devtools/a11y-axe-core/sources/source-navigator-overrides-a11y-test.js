// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the Sources panel Navigator pane Overrides tab using axe-core.');

  // axe-core issue #1444 -- role="tree" requires children with role="treeitem",
  // but it is reasonable to have trees with no leaves.
  const NO_REQUIRED_CHILDREN_RULESET = {
    'aria-required-children': {
      enabled: false,
    },
  };


  await UI.ViewManager.ViewManager.instance().showView('sources');

  await setup();
  await testA11yForView(NO_REQUIRED_CHILDREN_RULESET);

  TestRunner.completeTest();

  async function setup() {
    // Add an overrides folder
    await BindingsTestRunner.createOverrideProject('file:///tmp/');
  }

  async function testA11yForView(ruleSet) {
    await UI.ViewManager.ViewManager.instance().showView('navigator-overrides');
    const sourcesNavigatorView = new Sources.SourcesNavigator.OverridesNavigatorView();

    sourcesNavigatorView.show(UI.InspectorView.InspectorView.instance().element);
    SourcesTestRunner.dumpNavigatorView(sourcesNavigatorView);
    const element = Sources.SourcesPanel.SourcesPanel.instance().navigatorTabbedLocation.tabbedPane().element;
    await AxeCoreTestRunner.runValidation(element, ruleSet);
  }
})();
