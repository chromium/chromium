// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Application from 'devtools/panels/application/application.js';
import * as Common from 'devtools/core/common/common.js';
import * as SourceFrame from 'devtools/ui/legacy/components/source_frame/source_frame.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests Application Panel preview for resources of different types.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
      <img src="../resources/image.png">
    `);
  await TestRunner.evaluateInPagePromise(`
      function parse(val) {
          // This is here for the JSON file imported via the script tag below
      }
    `);
  await TestRunner.addScriptTag('../resources/json-value.js');

  function dump(node, prefix) {
    for (var child of node.children()) {
      TestRunner.addResult(prefix + child.listItemElement.textContent + (child.selected ? ' (selected)' : ''));
      dump(child, prefix + '  ');
    }
  }

  function dumpCurrentState(label) {
    var types = new Map([
      [SourceFrame.ResourceSourceFrame.ResourceSourceFrame, 'source'],
      [SourceFrame.ImageView.ImageView, 'image'],
      [SourceFrame.JSONView.JSONView, 'json']
    ]);

    var view = Application.ResourcesPanel.ResourcesPanel.instance();
    TestRunner.addResult(label);
    dump(view.sidebar.sidebarTree.rootElement(), '');
    var visibleView = view.visibleView;
    if (visibleView instanceof UI.SearchableView.SearchableView)
      visibleView = visibleView.children()[0];
    var typeLabel = 'unknown';
    for (var type of types) {
      if (!(visibleView instanceof type[0]))
        continue;
      typeLabel = type[1];
      break;
    }
    console.log('visible view: ' + typeLabel);
  }

  async function revealResourceWithDisplayName(name) {
    var target = SDK.TargetManager.TargetManager.instance().primaryPageTarget();
    var model = target.model(SDK.ResourceTreeModel.ResourceTreeModel);
    var resource = null;
    for (var r of model.mainFrame.resources()) {
      if (r.displayName !== name)
        continue;
      resource = r;
      break;
    }

    if (!r) {
      TestRunner.addResult(name + ' was not found');
      return;
    }
    await Common.Revealer.reveal(r);
    dumpCurrentState('Revealed ' + name + ':');
  }

  await UI.ViewManager.ViewManager.instance().showView('resources');
  dumpCurrentState('Initial state:');
  await revealResourceWithDisplayName('json-value.js');
  await revealResourceWithDisplayName('image.png');
  await revealResourceWithDisplayName('resources-panel-resource-preview.js');

  TestRunner.completeTest();
})();
