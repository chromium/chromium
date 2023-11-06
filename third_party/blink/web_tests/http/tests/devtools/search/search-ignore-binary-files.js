// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Verify that search doesn't search in binary resources.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <img src="./resources/pink.jpg">
    `);

  await TestRunner.addScriptTag('search-ignore-binary-files.js');
  SourcesTestRunner.waitForScriptSource('pink.jpg', doSearch);

  function doSearch(next) {
    var scope = new SourcesModule.SourcesSearchScope.SourcesSearchScope();
    var searchConfig = new Workspace.SearchConfig.SearchConfig('sources.search-in-files', 'AAAAAAA', true, false);
    SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, TestRunner.completeTest.bind(TestRunner));
  }
})();
