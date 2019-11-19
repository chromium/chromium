// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that dirty fileSystem uiSourceCodes are bound to network.\n`);
  await TestRunner.loadModule('bindings_test_runner');
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/foo.js': null});

  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});
  var fsUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem);
  var { content } = await fsUISourceCode.requestContent();
  content = content.replace(/foo/g, 'bar');
  fsUISourceCode.setWorkingCopy(content);

  TestRunner.addScriptTag('resources/foo.js');
  var binding = await BindingsTestRunner.waitForBinding('foo.js');
  TestRunner.addResult('Binding created: ' + binding);

  TestRunner.completeTest();
})();
