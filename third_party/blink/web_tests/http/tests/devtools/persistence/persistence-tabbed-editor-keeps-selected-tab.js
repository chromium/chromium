// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that tab keeps selected as the persistence binding comes in.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');
  await TestRunner.showPanel('sources');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);
  fs.root.addFile('bar.js', 'window.bar = ()=>\'bar\';');
  await fs.reportCreatedPromise();

  var fsSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
  var networkSourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network);
  var barSourceCode = await TestRunner.waitForUISourceCode('bar.js');
  UI.panels.sources.showUISourceCode(barSourceCode, 0, 0);
  UI.panels.sources.showUISourceCode(networkSourceCode, 0, 0);
  // Open and select file system tab. Selection should stay here.
  UI.panels.sources.showUISourceCode(fsSourceCode, 0, 0);

  dumpTabs('Opened tabs before persistence binding:');
  testMapping.addBinding('foo.js');
  await BindingsTestRunner.waitForBinding('foo.js');
  dumpTabs('\nOpened tabs after persistence binding:');
  TestRunner.completeTest();

  function dumpTabs(title) {
    var tabbedPane = UI.panels.sources.sourcesView().editorContainer.tabbedPane;
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
