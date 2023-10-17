// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';

(async function() {
  TestRunner.addResult(`Verify that search across all files omits filesystem uiSourceCodes with binding to network.\n`);
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);
  var scope = new SourcesModule.SourcesSearchScope.SourcesSearchScope();
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

    dumpSearchResults,

    function addFileMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(next);
    },

    dumpSearchResults,
  ]);

  function dumpSearchResults(next) {
    var searchConfig = new Workspace.SearchConfig.SearchConfig('window.foo f:foo', true, false);
    SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
  }

  InspectorFrontendHost.searchInPath = function(requestId, path, query) {
    setTimeout(reply);

    function reply() {
      var paths = ['/var/www' + fsEntry.fullPath];
      Persistence.IsolatedFileSystemManager.IsolatedFileSystemManager.instance().onSearchCompleted(
          {data: {requestId: requestId, fileSystemPath: path, files: paths}});
    }
  };
})();
