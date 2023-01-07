// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`The test verifies that source maps are loaded if specified inside style tag.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.loadHTML(`
    <style>/*# sourceMappingURL=data:application/json;charset=utf-8;base64,ew0KICAidmVyc2lvbiI6IDMsDQogICJmaWxlIjogIiIsDQogICJzb3VyY2VzIjogWyIvZGV2dG9vbHMvc291cmNlcy9yZXNvdXJjZXMvZW1wdHkuc2NzcyJdLA0KICAibmFtZXMiOiBbXSwNCiAgIm1hcHBpbmdzIjogIiINCn0= */</style>
  `);
  SourcesTestRunner.showScriptSource('empty.scss', onSourceFrame);

  function onSourceFrame(sourceFrame) {
    TestRunner.addResult('Source mapping loaded.');
    TestRunner.completeTest();
    dumpSuggestions = SourcesTestRunner.dumpSuggestions.bind(SourcesTestRunner, sourceFrame.textEditor);
    TestRunner.runTestSuite(testSuite);
  }
})();
