// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that UISourceCodeFrames are highlighted based on their network UISourceCode.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/foo_js_without_extension');

  const testMapping = BindingsTestRunner.initializeTestMapping();
  const fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  fs.root.mkdir('devtools')
         .mkdir('persistence')
         .mkdir('resources')
         .addFile('foo_js_without_extension', '\n\nwindow.foo = ()=>\'foo\';\n');
  fs.reportCreated(function() {});

  const networkSourceCode = await TestRunner.waitForUISourceCode('foo_js_without_extension', Workspace.projectTypes.Network);
  const networkSourceFrame = await SourcesTestRunner.showUISourceCodePromise(networkSourceCode);
  TestRunner.addResult(`Network UISourceCodeFrame highlighter type: ${networkSourceFrame.highlighterType()}`);

  const fileSystemSourceCode = await TestRunner.waitForUISourceCode('foo_js_without_extension', Workspace.projectTypes.FileSystem);
  const fileSystemSourceFrame = await SourcesTestRunner.showUISourceCodePromise(fileSystemSourceCode);
  TestRunner.addResult(`FileSystem UISourceCodeFrame highlighter type: ${fileSystemSourceFrame.highlighterType()}`);

  TestRunner.addResult('Adding binding');
  await testMapping.addBinding('foo_js_without_extension');
  TestRunner.addResult(`FileSystem UISourceCodeFrame highlighter type: ${fileSystemSourceFrame.highlighterType()}`);

  TestRunner.addResult('Remove binding');
  await testMapping.removeBinding('foo_js_without_extension');
  TestRunner.addResult(`FileSystem UISourceCodeFrame highlighter type: ${fileSystemSourceFrame.highlighterType()}`);

  TestRunner.completeTest();
})();
