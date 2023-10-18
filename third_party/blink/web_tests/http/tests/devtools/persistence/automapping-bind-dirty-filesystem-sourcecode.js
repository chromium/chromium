// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that dirty fileSystem uiSourceCodes are bound to network.\n`);
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/foo.js': null});

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});
  var fsUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.FileSystem);
  var { content } = await fsUISourceCode.requestContent();
  content = content.replace(/foo/g, 'bar');
  fsUISourceCode.setWorkingCopy(content);

  TestRunner.addScriptTag('resources/foo.js');
  var binding = await BindingsTestRunner.waitForBinding('foo.js');
  TestRunner.addResult('Binding created: ' + binding);

  TestRunner.completeTest();
})();
