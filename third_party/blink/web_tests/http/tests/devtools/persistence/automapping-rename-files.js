// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that automapping is sane.\n`);

  // Disable default-running automapping so that it doesn't conflict
  // with AutomappingTest.
  BindingsTestRunner.initializeTestMapping();

  var foo_js = {content: 'console.log(\'foo.js!\');', time: null};

  var automappingTest = new BindingsTestRunner.AutomappingTest(Workspace.Workspace.WorkspaceImpl.instance());
  automappingTest.addNetworkResources({
    'http://example.com/path/foo.js': foo_js,
  });

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'scripts/foo.js': foo_js,
  });
  await new Promise(fulfill => fs.reportCreated(fulfill));

  await automappingTest.waitUntilMappingIsStabilized();

  TestRunner.markStep('Rename foo.js => bar.js');
  var fileUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.FileSystem);
  await fileUISourceCode.rename('bar.js');
  await automappingTest.waitUntilMappingIsStabilized();

  TestRunner.markStep('Rename bar.js => foo.js');
  fileUISourceCode.rename('foo.js');
  await automappingTest.waitUntilMappingIsStabilized();

  TestRunner.completeTest();
})();
