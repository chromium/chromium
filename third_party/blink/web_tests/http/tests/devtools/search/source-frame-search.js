// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests different types of search in SourceFrame\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/search.js');

  function dumpSearchResultsForConfig(sourceFrame, searchConfig) {
    var modifiers = [];
    if (searchConfig.isRegex)
      modifiers.push('regex');
    if (searchConfig.caseSensitive)
      modifiers.push('caseSensitive');
    var modifiersString = modifiers.length ? ' (' + modifiers.join(', ') + ')' : '';

    TestRunner.addResult('Running search test for query = ' + searchConfig.query + modifiersString + ':');
    sourceFrame.performSearch(searchConfig, false, false);

    var searchResults = sourceFrame.searchResults;
    for (var i = 0; i < searchResults.length; ++i) {
      var range = searchResults[i];
      var prefixRange = new TextUtils.TextRange(range.startLine, 0, range.startLine, range.startColumn);
      var postfixRange = new TextUtils.TextRange(
          range.endLine, range.endColumn, range.endLine, sourceFrame.textEditor.line(range.endLine).length);
      var prefix = sourceFrame.textEditor.text(prefixRange);
      var result = sourceFrame.textEditor.text(range);
      var postfix = sourceFrame.textEditor.text(postfixRange);
      TestRunner.addResult('  - ' + prefix + '<' + result + '>' + postfix);
    }
  }

  await UI.viewManager.showView('sources');
  SourcesTestRunner.showScriptSource('search.js', didShowScriptSource);

  function didShowScriptSource(sourceFrame) {
    TestRunner.runTestSuite([
      function testSearch(next) {
        var query = 'searchTestUniqueString';
        var searchConfig = new UI.SearchableView.SearchConfig(query, false, false);
        dumpSearchResultsForConfig(sourceFrame, searchConfig);
        next();
      },

      function testSearchCaseSensitive(next) {
        var query = 'SEARCHTestUniqueString';
        var searchConfig = new UI.SearchableView.SearchConfig(query, true, false);
        dumpSearchResultsForConfig(sourceFrame, searchConfig);
        next();
      },

      function testSearchRegex(next) {
        var query = 'searchTestUnique.*';
        var searchConfig = new UI.SearchableView.SearchConfig(query, false, true);
        dumpSearchResultsForConfig(sourceFrame, searchConfig);
        next();
      },

      function testSearchCaseSensitiveRegex(next) {
        var query = 'searchTestUnique.*';
        var searchConfig = new UI.SearchableView.SearchConfig(query, true, true);
        dumpSearchResultsForConfig(sourceFrame, searchConfig);
        next();
      },

      function testSearchConsequent(next) {
        var query = 'AAAAA';
        var searchConfig = new UI.SearchableView.SearchConfig(query, false, false);
        dumpSearchResultsForConfig(sourceFrame, searchConfig);
        next();
      }
    ]);
  }
})();
