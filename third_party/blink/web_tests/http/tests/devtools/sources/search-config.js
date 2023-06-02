// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests search query parsing.\n`);
  await TestRunner.loadLegacyModule('search');
  await TestRunner.showPanel('sources');

  function dumpParsedSearchQuery(query, isRegex) {
    var searchConfig = new Search.SearchConfig(query, true, isRegex);
    TestRunner.addResult('Dumping parsed search query [' + query + ']:');
    TestRunner.addResult(JSON.stringify(searchConfig.queries()));
  }

  dumpParsedSearchQuery('function', false);
  dumpParsedSearchQuery(' function', false);
  dumpParsedSearchQuery(' function file:js', false);
  dumpParsedSearchQuery('file:js   function   ', false);
  dumpParsedSearchQuery('\s', true);
  dumpParsedSearchQuery(' \s hello', true);
  TestRunner.completeTest();
})();
