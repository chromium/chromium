// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verifies that the sourceMap is in fact re-requested from network as SASS watchdog updates the CSS file.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`<link rel="stylesheet">`);
  await TestRunner.evaluateInPagePromise(`
      function addStyleSheet()
      {
          var link = document.querySelector("link");
          link.setAttribute("href", "./resources/styles-rerequest-sourcemap-on-watchdog.css");
      }
  `);

  TestRunner.cssModel.sourceMapManager().addEventListener(
      SDK.SourceMapManager.Events.SourceMapAttached, onInitialSourceMap);

  TestRunner.evaluateInPagePromise('addStyleSheet()');

  function onInitialSourceMap() {
    TestRunner.cssModel.sourceMapManager().removeEventListener(SDK.SourceMapManager.Events.SourceMapAttached, onInitialSourceMap);
    SourcesTestRunner.waitForScriptSource('styles-rerequest-sourcemap-on-watchdog.css', onCSSFile);
  }

  function onCSSFile(uiSourceCode) {
    TestRunner.addSniffer(SDK.SourceMapManager.prototype, 'sourceMapLoadedForTest', onSourceMapRerequested);
    uiSourceCode.addRevision(
        'div { color: blue; } /*# sourceMappingURL=styles-rerequest-sourcemap-on-watchdog.css.map */');
  }

  function onSourceMapRerequested() {
    TestRunner.addResult('SourceMap successfully re-requested.');
    TestRunner.completeTest();
  }
})();
