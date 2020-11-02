// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that color swatch positions are updated properly.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/color.css');

  SourcesTestRunner.showScriptSource('color.css', onSourceFrame);

  function onSourceFrame(sourceFrame) {
    var cssPlugin = sourceFrame._plugins.find(plugin => plugin instanceof Sources.CSSPlugin);
    TestRunner.addResult('Initial swatch positions:');
    SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);

    TestRunner.runTestSuite([
      function testEditSpectrum(next) {
        var swatch = sourceFrame.textEditor._codeMirrorElement.querySelector('devtools-color-swatch');
        swatch.shadowRoot.querySelector('.color-swatch-inner').click();
        cssPlugin._spectrum._innerSetColor(
            Common.Color.parse('#008000').hsva(), '', undefined /* colorName */, Common.Color.Format.HEX,
            ColorPicker.Spectrum._ChangeSource.Other);
        cssPlugin._swatchPopoverHelper.hide(true);
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testAddLine(next) {
        var start = TextUtils.TextRange.createFromLocation(0, 0);
        sourceFrame.textEditor.editRange(start, '/* New line */\n');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testDeleteLine(next) {
        var bodyLine = new TextUtils.TextRange(2, 0, 3, 0);
        sourceFrame.textEditor.editRange(bodyLine, '');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testAddColor(next) {
        var emptyBodyLine = new TextUtils.TextRange(2, 0, 2, 0);
        sourceFrame.textEditor.editRange(emptyBodyLine, 'color: hsl(300, 100%, 35%);');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testInvalidateColor(next) {
        var endParenthesis = new TextUtils.TextRange(2, 25, 2, 26);
        sourceFrame.textEditor.editRange(endParenthesis, ']');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      },

      function testBookmarksAtLineStart(next) {
        var lineStart = new TextUtils.TextRange(5, 0, 5, 0);
        sourceFrame.textEditor.editRange(lineStart, 'background color:\n#ff0;\n');
        SourcesTestRunner.dumpSwatchPositions(sourceFrame, Sources.CSSPlugin.SwatchBookmark);
        next();
      }
    ]);
  }
})();
