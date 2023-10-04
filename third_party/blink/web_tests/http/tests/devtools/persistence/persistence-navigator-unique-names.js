// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Verify that navigator view removes mapped UISourceCodes.\n`);

  var filesNavigator = new Sources.SourcesNavigator.FilesNavigatorView();
  filesNavigator.show(UI.InspectorView.InspectorView.instance().element);
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

  SourcesTestRunner.dumpNavigatorView(filesNavigator);
  TestRunner.completeTest();
})();
