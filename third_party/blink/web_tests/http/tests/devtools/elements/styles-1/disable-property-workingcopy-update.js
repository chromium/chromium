// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that style property disablement is propagated into the stylesheet UISourceCode working copy.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="inspected">
      </div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/disable-property-workingcopy-update.css');

  var cssSourceFrame;
  Bindings.StylesSourceMapping.MinorChangeUpdateTimeoutMs = 10;

  TestRunner.runTestSuite([
    function selectContainer(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function showEditor(next) {
      var headers = TestRunner.cssModel.styleSheetHeaders();
      for (var i = 0; i < headers.length; ++i) {
        if (headers[i].sourceURL.endsWith('.css')) {
          var cssLocation = new SDK.CSSLocation(headers[i], 0);
          SourcesTestRunner.showUISourceCode(
              Bindings.cssWorkspaceBinding.rawLocationToUILocation(cssLocation).uiSourceCode, callback);
          break;
        }
      }

      function callback(sourceFrame) {
        cssSourceFrame = sourceFrame;
        SourcesTestRunner.dumpSourceFrameContents(cssSourceFrame);
        next();
      }
    },

    function togglePropertyOff(next) {
      toggleProperty(false, next);
    },

    async function dumpDisabledText(next) {
      SourcesTestRunner.dumpSourceFrameContents(cssSourceFrame);
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    },

    function togglePropertyOn(next) {
      toggleProperty(true, next);
    },

    async function dumpEnabledText(next) {
      SourcesTestRunner.dumpSourceFrameContents(cssSourceFrame);
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    }
  ]);

  function toggleProperty(value, next) {
    TestRunner.addSniffer(Workspace.UISourceCode.prototype, 'addRevision', callback);
    ElementsTestRunner.waitForStyles('inspected', callback);
    ElementsTestRunner.toggleMatchedStyleProperty('font-weight', value);

    var barrierCounter = 2;
    function callback() {
      if (!--barrierCounter)
        next();
    }
  }
})();
