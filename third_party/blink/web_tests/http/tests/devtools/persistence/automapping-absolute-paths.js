// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that automapping is capable of mapping file:// urls.\n`);

  var app_js = {content: 'console.log(\'foo.js!\');', time: null};

  var automappingTest = new BindingsTestRunner.AutomappingTest(new Workspace.Workspace.WorkspaceImpl());
  automappingTest.addNetworkResources({
    // Make sure main resource gets mapped.
    'file:///usr/local/node/app.js': app_js,
  });

  var fs = new BindingsTestRunner.TestFileSystem('/usr/local/node');
  BindingsTestRunner.addFiles(fs, {
    'app.js': app_js,
  });
  fs.reportCreated(onFileSystemCreated);

  function onFileSystemCreated() {
    automappingTest.waitUntilMappingIsStabilized().then(TestRunner.completeTest.bind(TestRunner));
  }
})();
