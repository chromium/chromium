// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as QuickOpen from 'devtools/ui/legacy/components/quick_open/quick_open.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Verify that GoTo source dialog filters out mapped uiSourceCodes.\n`);
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});

  TestRunner.runTestSuite([
    function waitForUISourceCodes(next) {
      Promise
          .all([
            TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.Network),
            TestRunner.waitForUISourceCode('foo.js', Workspace.Workspace.projectTypes.FileSystem)
          ])
          .then(next);
    },

    function goToSourceDialogBeforeBinding(next) {
      dumpGoToSourceDialog(next);
    },

    function addFileSystemMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(next);
    },

    function goToSourceAfterBinding(next) {
      dumpGoToSourceDialog(next);
    },
  ]);

  function dumpGoToSourceDialog(next) {
    TestRunner.addSnifferPromise(QuickOpen.QuickOpen.QuickOpenImpl.prototype, 'providerLoadedForTest').then(provider => {
      var keys = [];
      for (var i = 0; i < provider.itemCount(); ++i)
        keys.push(provider.itemKeyAt(i));
      keys.sort();
      TestRunner.addResult(keys.join('\n'));
      UIModule.Dialog.Dialog.instance.hide();
      next();
    });
    QuickOpen.QuickOpen.QuickOpenImpl.show('');
  }
})();
