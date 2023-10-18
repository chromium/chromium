// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  'use strict';
  TestRunner.addResult(
      `Verify that a network file tab gets substituted with filesystem tab when persistence binding comes.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  TestRunner.runTestSuite([
    function openNetworkTab(next) {
      TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.Network)
          .then(sourceCode => SourcesTestRunner.showUISourceCodePromise(sourceCode))
          .then(onSourceFrame);

      function onSourceFrame(sourceFrame) {
        dumpEditorTabs();
        next();
      }
    },

    function addMapping(next) {
      var fs = new BindingsTestRunner.TestFileSystem('/var/www');
      BindingsTestRunner.addFooJSFile(fs);
      fs.reportCreated(function() {});
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

      function onBindingCreated() {
        dumpEditorTabs();
        next();
      }
    },
  ]);

  function dumpEditorTabs() {
    var editorContainer = Sources.SourcesPanel.SourcesPanel.instance().sourcesView().editorContainer;
    var openedUISourceCodes = [...editorContainer.tabIds.keys()];
    openedUISourceCodes.sort((a, b) => a.url > b.url ? 1 : b.url > a.url ? -1 : 0);
    TestRunner.addResult('Opened tabs: ');
    for (const code of openedUISourceCodes)
      TestRunner.addResult('    ' + code.url());
  }
})();
