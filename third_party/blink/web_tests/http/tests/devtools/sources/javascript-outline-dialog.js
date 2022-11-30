// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify JavaScriptOutlineDialog scoring.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/javascript-outline-dialog.js');

  SourcesTestRunner.showScriptSource('javascript-outline-dialog.js', onSourceShown);
  var provider;
  function onSourceShown(sourceFrame) {
    TestRunner.addSniffer(Sources.OutlineQuickOpen.prototype, 'refresh', onQuickOpenFulfilled);
    UI.panels.sources.sourcesView().showOutlineQuickOpen();
  }

  function onQuickOpenFulfilled() {
    provider = this;
    dumpScores('te');
    dumpScores('test');
    dumpScores('test(');
    dumpScores('test(arg');
    TestRunner.completeTest();
  }

  function dumpScores(query) {
    TestRunner.addResult(`Scores for query="${query}"`);
    var keys = [];
    for (var i = 0; i < provider.itemCount(); ++i) {
      keys.push({key: provider.itemKeyAt(i), score: provider.itemScoreAt(i, query)});
    }
    keys.sort((a, b) => b.score - a.score);
    TestRunner.addResult(keys.map(a => a.key + ' ' + a.score).join('\n'));
    TestRunner.addResult('');
  }
})();
