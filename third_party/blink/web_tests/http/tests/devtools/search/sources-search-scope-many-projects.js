// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that ScriptSearchScope sorts network and dirty results correctly.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('search');
  await TestRunner.showPanel('sources');

  function fileSystemUISourceCodes() {
    var uiSourceCodes = [];
    var fileSystemProjects = Workspace.workspace.projectsForType(Workspace.projectTypes.FileSystem);
    for (var project of fileSystemProjects)
      uiSourceCodes = uiSourceCodes.concat(project.uiSourceCodes());
    return uiSourceCodes;
  }

  var scope = new Sources.SourcesSearchScope();
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

    await SourcesTestRunner.addScriptUISourceCode('http://localhost/search.html', resources['search.html']);
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
      Persistence.isolatedFileSystemManager.onSearchCompleted(
          {data: {requestId: requestId, fileSystemPath: path, files: paths}});
    }
  };

  var testSuite = [
    function testSearch(next) {
      var query = 'searchTest' +
          'UniqueString';
      var searchConfig = new Search.SearchConfig(query, true, false);
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
      var searchConfig = new Search.SearchConfig(query, true, false);
      SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, next);
    }
  ];
})();
