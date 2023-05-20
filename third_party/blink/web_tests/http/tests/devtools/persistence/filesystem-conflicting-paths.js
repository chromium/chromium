// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that uiSourceCode.delete actually deltes file from IsolatedFileSystem.\n`);

  var fs1 = new BindingsTestRunner.TestFileSystem('/var/www');
  var file1 = fs1.addFile('foo.js', 'foo.js', 0);
  var fs2 = new BindingsTestRunner.TestFileSystem('/var/www_suffix');
  var file2 = fs2.addFile('bar.js', 'bar.js', 0);
  await new Promise(x => fs1.reportCreated(x));
  await new Promise(x => fs2.reportCreated(x));
  await TestRunner.waitForUISourceCode('foo.js');

  TestRunner.addResult('\n== Initial workspace ==');
  var snapshot = BindingsTestRunner.dumpWorkspace();
  file2.setContent('Why!?');

  TestRunner.addResult('\n== After changing file content ==');
  BindingsTestRunner.dumpWorkspace(snapshot);
  TestRunner.completeTest();
})();
