// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      `Verify that tabbed editor doesn't shuffle tabs when bindings are dropped and then re-added during reload.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(TestRunner.url('resources/persistence-tabbed-editor-tab-order.html'));

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var folder = fs.root.mkdir('devtools').mkdir('persistence').mkdir('resources');
  folder.addFile('foo.js', '\n\nwindow.foo = ()=>\'foo\';');
  folder.addFile('bar.js', 'window.bar = () => "bar";');
  folder.addFile('baz.js', 'window.baz = () => "baz";');
  fs.reportCreated(function() {});

  TestRunner.runTestSuite([
    async function waitForBindings(next) {
      testMapping.addBinding('foo.js');
      testMapping.addBinding('bar.js');
      testMapping.addBinding('baz.js');
      await Promise.all([
        BindingsTestRunner.waitForBinding('foo.js'),
        BindingsTestRunner.waitForBinding('bar.js'),
        BindingsTestRunner.waitForBinding('baz.js'),
      ]);
      next();
    },

    async function openNetworkFiles(next) {
      var uiSourceCodes = await Promise.all([
        TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.Network),
        TestRunner.waitForUISourceCode('bar.js', Workspace.Workspace.projectTypes.Network),
        TestRunner.waitForUISourceCode('baz.js', Workspace.Workspace.projectTypes.Network)
      ]);

      for (var uiSourceCode of uiSourceCodes)
        SourcesTestRunner.showUISourceCode(uiSourceCode, function() {});
      dumpTabs('initial tabs:');
      next();
    },

    async function reloadPage(next) {
      await new Promise(x => TestRunner.hardReloadPage(x));
      await Promise.all([
        BindingsTestRunner.waitForBinding('foo.js'),
        BindingsTestRunner.waitForBinding('bar.js'),
        BindingsTestRunner.waitForBinding('baz.js'),
      ]);
      dumpTabs('Tabs after reload:');
      next();
    },
  ]);

  function dumpTabs(title) {
    var tabbedPane = Sources.SourcesPanel.SourcesPanel.instance().sourcesView().editorContainer.tabbedPane;
    var tabs = tabbedPane.tabs;
    TestRunner.addResult(title);
    for (var i = 0; i < tabs.length; ++i) {
      var text = (i + 1) + ': ';
      text += tabs[i].title;
      if (tabs[i] === tabbedPane.currentTab)
        text += ' [selected]';
      TestRunner.addResult('    ' + text);
    }
  }
})();
