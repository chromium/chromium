// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests different types of search-and-replace in SourceFrame\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/search.js');

  await UI.viewManager.showView('sources');
  SourcesTestRunner.showScriptSource('search.js', didShowScriptSource);

  function didShowScriptSource(sourceFrame) {
    var searchConfig = new UI.SearchableView.SearchConfig('(REPLACE)ME[38]', true, true);
    SourcesTestRunner.replaceAndDumpChange(sourceFrame, searchConfig, '$1D', false);

    var searchConfig = new UI.SearchableView.SearchConfig('(REPLACE)ME[45]', true, true);
    SourcesTestRunner.replaceAndDumpChange(sourceFrame, searchConfig, '$1D', true);

    TestRunner.completeTest();
  }
})();
