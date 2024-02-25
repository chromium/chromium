// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests script search in inspector debugger agent.\n`);
  await TestRunner.showPanel('sources');

  await TestRunner.addIframe('resources/search.html');

  SourcesTestRunner.startDebuggerTest(step1);
  var script;

  function step1() {
    ApplicationTestRunner.runAfterResourcesAreFinished(['search.js'], step2);
  }

  function step2() {
    SourcesTestRunner.showScriptSource('search.js', step3);
  }

  async function step3() {
    var url = 'http://127.0.0.1:8000/devtools/search/resources/search.js';
    var scripts = SourcesTestRunner.queryScripts(function(s) {
      return s.sourceURL === url;
    });
    script = scripts[0];
    TestRunner.addResult(script.sourceURL);

    // This file should not match search query.
    var text = 'searchTest' +
        'UniqueString';
    var searchMatches = await script.searchInContent(text, false, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    // This file should not match search query.
    var text = 'searchTest' +
        'UniqueString';
    searchMatches = await script.searchInContent(text, true, false);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await script.searchInContent(text, false, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    var text = '[a-z]earchTestUniqueString';
    searchMatches = await script.searchInContent(text, true, true);
    SourcesTestRunner.dumpSearchMatches(searchMatches);

    SourcesTestRunner.completeDebuggerTest();
  }
})();
