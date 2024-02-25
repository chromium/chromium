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
  TestRunner.addResult(`Tests that ScriptSearchScope performs search across all sources correctly.\n`);
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
  var names = ['search.html', 'search.js', 'search.css'];
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');

  var promises = [];
  for (var name of names)
    promises.push(populateFileSystem(name));

  Promise.all(promises).then(onAllResourcesLoaded).catch(onResourceError);

  function onResourceError(error) {
    TestRunner.addResult('ERROR while loading resources: ' + error.message);
    TestRunner.completeTest();
  }

  function onAllResourcesLoaded() {
    fs.reportCreated(fileSystemCreated);

    function fileSystemCreated() {
      TestRunner.addResult('Total uiSourceCodes: ' + workspace.uiSourceCodes().length);
      TestRunner.runTestSuite(testSuite);
    }
  }

  function populateFileSystem(name) {
    var url = TestRunner.url('resources/' + name);
    return fetch(url).then(result => result.text()).then(function(text) {
      fs.root.addFile(name, text);
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
    function testIgnoreCase(next) {
      var query = 'searchTest' +
          'UniqueString';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testCaseSensitive(next) {
      var query = 'searchTest' +
          'UniqueString';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, false, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testFileHTML(next) {
      var query = 'searchTest' +
          'UniqueString' +
          ' file:html';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testFileJS(next) {
      var query = 'file:js ' +
          'searchTest' +
          'UniqueString';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testFileHTMLJS(next) {
      var query = 'file:js ' +
          'searchTest' +
          'UniqueString' +
          ' file:html';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSpaceQueries(next) {
      var query = 'searchTest' +
          'Unique' +
          ' space' +
          ' String';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSpaceQueriesFileHTML(next) {
      var query = 'file:html ' +
          'searchTest' +
          'Unique' +
          ' space' +
          ' String';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSpaceQueriesFileHTML_SEARCH(next) {
      var query = 'file:html ' +
          'searchTest' +
          'Unique' +
          ' space' +
          ' String' +
          ' file:search';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSpaceQueriesFileJS_SEARCH_HTML(next) {
      var query = 'file:js ' +
          'searchTest' +
          'Unique' +
          ' space' +
          ' String' +
          ' file:search file:html';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSeveralQueriesFileHTML(next) {
      var query = 'searchTest' +
          'Unique' +
          ' file:html ' +
          ' space' +
          ' String';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSeveralQueriesFileHTML_SEARCH(next) {
      var query = 'searchTest' +
          'Unique' +
          ' file:html ' +
          ' space' +
          ' String' +
          ' file:search';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSeveralQueriesFileJS_SEARCH_HTML(next) {
      var query = 'file:js ' +
          'searchTest' +
          'Unique' +
          ' file:html ' +
          ' space' +
          ' String' +
          ' file:search';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testFileSEARCH_NOT_JS_NOT_CSS(next) {
      var query = 'searchTest' +
          'UniqueString' +
          ' file:search -file:js -file:css';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testSeveralQueriesFileNotCSS(next) {
      var query = 'searchTest' +
          'Unique' +
          ' -file:css ' +
          ' space' +
          ' String';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    },

    function testFileQueryWithProjectName(next) {
      TestRunner.addResult('Running a file query with existing project name first:');
      var query = 'searchTest' +
          'Unique' +
          ' file:www';
      var searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, step2);

      function step2() {
        TestRunner.addResult('Running a file query with non-existing project name now:');
        query = 'searchTest' +
            'Unique' +
            ' file:zzz';
        searchConfig = new Workspace.SearchConfig.SearchConfig(query, true, false);
        SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
      }
    },

    function testDirtyFiles(next) {
      var uiSourceCode;
      var uiSourceCodes = fileSystemUISourceCodes();
      for (var i = 0; i < uiSourceCodes.length; ++i) {
        if (uiSourceCodes[i].name() === 'search.js') {
          uiSourceCode = uiSourceCodes[i];
          break;
        }
      }

      uiSourceCode.setWorkingCopy(
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
