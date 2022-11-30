// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that inline stylesheets do not appear in navigator.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      </style>
    `);
  await TestRunner.evaluateInPageAnonymously(`
      function injectStyleSheet()
      {
          var style = document.createElement('style');
          style.textContent = '* {color: blue; }';
          document.head.appendChild(style);
      }
  `);

  Promise.all([UI.inspectorView.showPanel('sources'), TestRunner.evaluateInPageAnonymously('injectStyleSheet()')])
      .then(onInjected);

  function onInjected() {
    var sourcesNavigator = new Sources.NetworkNavigatorView();
    SourcesTestRunner.dumpNavigatorView(sourcesNavigator);
    TestRunner.completeTest();
  }
})();
