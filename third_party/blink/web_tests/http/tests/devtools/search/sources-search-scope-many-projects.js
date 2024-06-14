// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';

(async function() {
  TestRunner.addResult(`Tests that ScriptSearchScope sorts network and dirty results correctly.\n`);
  await TestRunner.showPanel('sources');

  const workspace = Workspace.Workspace.WorkspaceImpl.instance();

  function fileSystemUISourceCodes() {
    var uiSourceCodes = [];
    var fileSystemProjects = workspace.projectsForType(Workspace.Workspace.projectTypes.FileSystem);
    for (var project of fileSystemProjects) {
      for (const uiSourceCode of project.uiSourceCodes()) {
        uiSourceCodes.push(uiSourceCode);
      }
    }
    return uiSourceCodes;
  }

  var scope = new SourcesModule.SourcesSearchScope.SourcesSearchScope();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var names = ['search.html', 'search.js', 'search.css'];
  var resources = {};
  var jsFileSystemUISourceCode;
  var jsNetworkUISourceCode;

  var promises = [];
  for (var name of names)
    promises.push(loadResource(name));

  Promise.all(promises).then(onAllResourcesLoaded).catch(onResourceError);

  function onResourceError(error) {
    TestRunner.addResult('ERROR while loading resources: ' + error.message);
    TestRunner.completeTest();
  }

  function onAllResourcesLoaded() {
    for (var resourceName in resources)
      fs.root.addFile(resourceName, resources[resourceName]);
    fs.reportCreated(fileSystemCreated);
  }

  async function fileSystemCreated() {
    var uiSourceCodes = fileSystemUISourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
      if (uiSourceCodes[i].name() === 'search.js') {
        jsFileSystemUISourceCode = uiSourceCodes[i];
        break;
      }
    }

    jsNetworkUISourceCode =
        await SourcesTestRunner.addScriptUISourceCode('http://localhost/search.js', resources['search.js']);
    TestRunner.runTestSuite(testSuite);
  }

  function loadResource(name) {
    var url = TestRunner.url('resources/' + name);
    return fetch(url).then(result => result.text()).then(function(text) {
      resources[name] = text;
    });
  }

  InspectorFrontendHost.searchInPath = function(requestId, path, query) {
    setTimeout(reply);

    function reply() {
      var paths = [];
      for (var i = 0; i < names.length; ++i)
        paths.push('/var/www/' + names[i]);
      Persistence.IsolatedFileSystemManager.IsolatedFileSystemManager.instance().onSearchCompleted(
          {data: {requestId: requestId, fileSystemPath: path, files: paths}});
    }
  };

  var testSuite = [
    function testSearch(next) {
      var query = 'searchTest' +
          'UniqueString';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testDirtyFiles(next) {
      jsFileSystemUISourceCode.setWorkingCopy(
          'FOO ' +
          'searchTest' +
          'UniqueString' +
          ' BAR');
      jsNetworkUISourceCode.setWorkingCopy(
          'FOO ' +
          'searchTest' +
          'UniqueString' +
          ' BAR');

      var query = 'searchTest' +
          'UniqueString';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    }
  ];
})();
