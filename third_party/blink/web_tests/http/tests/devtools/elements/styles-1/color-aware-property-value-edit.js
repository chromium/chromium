// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that property value being edited uses the user-specified color format.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected1" style="border: 1px solid red">inspected1</div>
      <div id="inspected2" style="color: #ffffee">inspected2</div>
    `);

  TestRunner.runTestSuite([
    function init(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected1', next);
    },

    function editKeywordAsOriginal(next) {
      startEditingAndDumpValue(Common.Color.Format.Original, 'border', next);
    },

    function editKeywordAsHex(next) {
      startEditingAndDumpValue(Common.Color.Format.HEX, 'border', next);
    },

    function editKeywordAsHSL(next) {
      startEditingAndDumpValue(Common.Color.Format.HSL, 'border', next);
    },

    function editKeywordAsRGB(next) {
      startEditingAndDumpValue(Common.Color.Format.RGB, 'border', onValueDumped);
      function onValueDumped() {
        ElementsTestRunner.selectNodeAndWaitForStyles('inspected2', next);
      }
    },

    function editHexAsOriginal(next) {
      startEditingAndDumpValue(Common.Color.Format.Original, 'color', next);
    },

    function editHexAsHex(next) {
      startEditingAndDumpValue(Common.Color.Format.HEX, 'color', next);
    },

    function editHexAsHSL(next) {
      startEditingAndDumpValue(Common.Color.Format.HSL, 'color', next);
    },

    function editHexAsRGB(next) {
      startEditingAndDumpValue(Common.Color.Format.RGB, 'color', next);
    },

    async function editNewProperty(next) {
      var section = ElementsTestRunner.inlineStyleSection();

      treeElement = section.addNewBlankProperty(0);
      treeElement.startEditing();
      treeElement.nameElement.textContent = 'border-color';
      treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
      treeElement.valueElement.textContent = 'hsl(120, 100%, 25%)';
      await treeElement.kickFreeFlowStyleEditForTest();

      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Tab', false, false, true));
      treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Tab'));
      TestRunner.addResult(treeElement.valueElement.textContent);
      next();
    }
  ]);

  function setFormat(newFormat, callback) {
    Common.settingForTest('colorFormat').set(newFormat);
    UI.panels.elements._stylesWidget.doUpdate().then(callback);
  }

  function startEditingAndDumpValue(format, propertyName, next) {
    setFormat(format, onFormatSet);

    function onFormatSet() {
      var treeElement = ElementsTestRunner.getElementStylePropertyTreeItem(propertyName);
      treeElement.startEditing(treeElement.valueElement);
      TestRunner.addResult(treeElement.valueElement.textContent);
      treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Escape'));
      next();
    }
  }
})();
