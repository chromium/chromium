// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that bezier swatches are updated properly in CSS Sources.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/bezier.css');

  SourcesTestRunner.showScriptSource('bezier.css', onSourceFrame);

  function onSourceFrame(sourceFrame) {
    var cssPlugin = sourceFrame.plugins.find(plugin => plugin instanceof Sources.CSSPlugin);
    TestRunner.addResult('Initial swatch positions:');
    SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);

    TestRunner.runTestSuite([
      function testEditBezier(next) {
        var swatch = sourceFrame.textEditor.codeMirrorElement.querySelector('span[is=bezier-swatch]');
        swatch.shadowRoot.querySelector('.bezier-swatch-icon').click();
        cssPlugin.bezierEditor.setBezier(UI.Geometry.CubicBezier.parse('linear'));
        cssPlugin.bezierEditor.onchange();
        cssPlugin.swatchPopoverHelper.hide(true);
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testAddBezier(next) {
        var bodyLineEnd = new TextUtils.TextRange(1, 37, 1, 37);
        sourceFrame.textEditor.editRange(bodyLineEnd, ' transition: height 1s cubic-bezier(0, 0.5, 1, 1);');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testInvalidateBezier(next) {
        var startParenthesis = new TextUtils.TextRange(1, 67, 1, 68);
        sourceFrame.textEditor.editRange(startParenthesis, '[');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      }
    ]);
  }
})();
