// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that source map is not detached on edits. crbug.com/257778\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">container.</div>
    `);
  await TestRunner.addStylesheetTag('./resources/styles-do-not-detach-sourcemap-on-edits.css');

  SourcesTestRunner.waitForScriptSource('styles-do-not-detach-sourcemap-on-edits.scss', onSourceMapLoaded);

  function onSourceMapLoaded() {
    ElementsTestRunner.selectNodeAndWaitForStyles('container', onNodeSelected);
  }

  function onNodeSelected() {
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    async function editProperty(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

      var treeItem = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
      treeItem.applyStyleText('NAME: VALUE', true);
      ElementsTestRunner.waitForStyles('container', next);
    },

    async function editSelector(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

      var section = ElementsTestRunner.firstMatchedStyleSection();
      section.startEditingSelector();
      section.selectorElement.textContent = '#container, SELECTOR';
      ElementsTestRunner.waitForSelectorCommitted(next);
      section.selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    async function editMedia(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

      var section = ElementsTestRunner.firstMatchedStyleSection();
      var mediaTextElement = ElementsTestRunner.firstMediaTextElementInSection(section);
      mediaTextElement.click();
      mediaTextElement.textContent = '(max-width: 9999999px)';
      ElementsTestRunner.waitForMediaTextCommitted(next);
      mediaTextElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    },

    async function addRule(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

      var styleSheetHeader = TestRunner.cssModel.styleSheetHeaders().find(
          header => header.resourceURL().indexOf('styles-do-not-detach-sourcemap-on-edits.css') !== -1);
      if (!styleSheetHeader) {
        TestRunner.addResult('ERROR: failed to find style sheet!');
        TestRunner.completeTest();
        return;
      }
      ElementsTestRunner.addNewRuleInStyleSheet(styleSheetHeader, 'NEW-RULE', next);
    },

    async function finish(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
      next();
    },
  ];
})();
