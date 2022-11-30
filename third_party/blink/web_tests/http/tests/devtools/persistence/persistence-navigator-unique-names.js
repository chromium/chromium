// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator view removes mapped UISourceCodes.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');

  var filesNavigator = new Sources.FilesNavigatorView();
  filesNavigator.show(UI.inspectorView.element);
  var fs1 = new BindingsTestRunner.TestFileSystem('/home/workspace/good/foo/bar');
  fs1.addFile('1.js', '');
  fs1.reportCreated(function() { });

  var fs2 = new BindingsTestRunner.TestFileSystem('/home/workspace/bad/foo/bar');
  fs2.addFile('2.js', '');
  fs2.reportCreated(function(){ });

  var fs3 = new BindingsTestRunner.TestFileSystem('/home/workspace/ugly/bar');
  fs3.addFile('3.js', '');
  fs3.reportCreated(function(){ });

  await Promise.all([
    TestRunner.waitForUISourceCode('1.js'),
    TestRunner.waitForUISourceCode('2.js'),
    TestRunner.waitForUISourceCode('3.js')
  ]);

  SourcesTestRunner.dumpNavigatorView(filesNavigator, true);
  TestRunner.completeTest();
})();
