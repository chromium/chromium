// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Verify that tabs get merged when binding is added and removed.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);
  var networkSourceFrame, fileSystemSourceFrame;

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    function openNetworkTab(next) {
      TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network)
          .then(code => SourcesTestRunner.showUISourceCodePromise(code))
          .then(onNetworkTab);

      function onNetworkTab(sourceFrame) {
        networkSourceFrame = sourceFrame;
        networkSourceFrame.setSelection(new TextUtils.TextRange(2, 0, 2, 5));
        networkSourceFrame.scrollToLine(2);
        dumpSourceFrame(networkSourceFrame);
        next();
      }
    },

    function openFileSystemTab(next) {
      TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
          .then(code => SourcesTestRunner.showUISourceCodePromise(code))
          .then(onFileSystemTab);

      function onFileSystemTab(sourceFrame) {
        fileSystemSourceFrame = sourceFrame;
        fileSystemSourceFrame.setSelection(new TextUtils.TextRange(1, 0, 2, 5));
        fileSystemSourceFrame.scrollToLine(1);
        dumpSourceFrame(fileSystemSourceFrame);
        dumpEditorTabs();
        next();
      }
    },

    function addFileMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

      function onBindingCreated() {
        dumpEditorTabs();
        dumpSourceFrame(fileSystemSourceFrame);
        next();
      }
    },

    function removeFileMapping(next) {
      Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
      testMapping.removeBinding('foo.js');

      function onBindingRemoved(event) {
        var binding = event.data;
        if (binding.network.name() !== 'foo.js')
          return;
        Persistence.persistence.removeEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
        dumpEditorTabs();
        dumpSourceFrame(fileSystemSourceFrame);
        next();
      }
    },
  ]);

  function dumpEditorTabs() {
    var editorContainer = UI.panels.sources._sourcesView._editorContainer;
    var openedUISourceCodes = [...editorContainer._tabIds.keys()];
    openedUISourceCodes.sort((a, b) => a.url() > b.url() ? 1 : b.url() > a.url() ? -1 : 0);
    TestRunner.addResult('Opened tabs: ');
    for (const code of openedUISourceCodes)
      TestRunner.addResult('    ' + code.url());
  }

  function dumpSourceFrame(sourceFrame) {
    TestRunner.addResult('SourceFrame: ' + sourceFrame.uiSourceCode().url());
    TestRunner.addResult('    selection: ' + sourceFrame.selection());
    TestRunner.addResult('    firstVisibleLine: ' + sourceFrame.textEditor.firstVisibleLine());
    TestRunner.addResult('    isDirty: ' + sourceFrame.uiSourceCode().isDirty());
  }
})();
