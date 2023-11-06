// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that dirty network uiSourceCodes are bound to filesystem.\n`);
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/foo.js': null});
  TestRunner.addScriptTag('resources/foo.js');
  var networkUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.Network);
  var { content } = await networkUISourceCode.requestContent();
  content = content.replace(/foo/g, 'bar');
  networkUISourceCode.setWorkingCopy(content);

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});

  var binding = await BindingsTestRunner.waitForBinding('foo.js');
  TestRunner.addResult('Binding created: ' + binding);
  TestRunner.addResult('FileSystem is dirty: ' + binding.fileSystem.isDirty());
  TestRunner.addResult('FileSystem working copy: ');
  TestRunner.addResult(binding.fileSystem.workingCopy());

  TestRunner.completeTest();
})();
