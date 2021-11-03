// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests different types of search-and-replace in SourceFrame\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/search.js');

  await UI.viewManager.showView('sources');

  SourcesTestRunner.showScriptSource('search.js', didShowScriptSource);

  function didShowScriptSource(sourceFrame) {
    var searchConfig = new UI.SearchableView.SearchConfig('replaceMe1', false, false);
    SourcesTestRunner.replaceAndDumpChange(sourceFrame, searchConfig, 'replaced', false);

    var searchConfig = new UI.SearchableView.SearchConfig('replaceMe2', false, false);
    SourcesTestRunner.replaceAndDumpChange(sourceFrame, searchConfig, 'replaced', true);

    TestRunner.completeTest();
  }
})();
