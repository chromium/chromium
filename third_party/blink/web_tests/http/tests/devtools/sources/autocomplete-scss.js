// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`The test verifies autocomplete suggestions for SCSS file.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('./resources/empty.css');

  SourcesTestRunner.showScriptSource('empty.scss', onSourceFrame);

  var dumpSuggestions;
  function onSourceFrame(sourceFrame) {
    dumpSuggestions = SourcesTestRunner.dumpSuggestions.bind(SourcesTestRunner, sourceFrame.textEditor);
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    function testPropertyValueSuggestionsBefore$(next) {
      dumpSuggestions(['@mixin my-border-style($style) {', '    border-style: |$;', '}']).then(next);
    },

    function testPropertyValueSuggestionsAfter$(next) {
      dumpSuggestions(['@mixin my-border-style($style) {', '    border-style: $|;', '}']).then(next);
    },
  ];
})();
