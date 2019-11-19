// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that sourcemap sources are mapped with non-exact match.\n`);
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('./resources/s.css');

  BindingsTestRunner.initializeTestMapping();
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/s.css': null});

  Promise.all([getResourceContent('s.css'), getResourceContent('s.scss')]).then(onResourceContents);

  function onResourceContents(contents) {
    var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
    BindingsTestRunner.addFiles(fs, {
      'dist/s.css': {content: contents[0], time: new Date('December 1, 1989')},
      'src/s.scss': {content: contents[1], time: new Date('December 1, 1989')}
    });
    fs.reportCreated(onFileSystemCreated);
  }

  function onFileSystemCreated() {
    var automappingTest = new BindingsTestRunner.AutomappingTest(Workspace.workspace);
    automappingTest.waitUntilMappingIsStabilized().then(TestRunner.completeTest.bind(TestRunner));
  }

  function getResourceContent(name) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    SourcesTestRunner.waitForScriptSource(name, onSource);
    return promise;

    function onSource(uiSourceCode) {
      uiSourceCode.requestContent().then(({ content, error, isEncoded }) => fulfill(content));
    }
  }
})();
