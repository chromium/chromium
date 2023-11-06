// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verifies that formatter adds a semicolon when enabling property.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      #formatted {
          color: red;
          margin: 0
      }

      </style>
      <div id="formatted">Formatted</div>
    `);

  var formattedStyle;

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, onStyleSheetChanged, this);

  function onStyleSheetChanged(event) {
    if (!event.data || !event.data.edit)
      return;
    formattedStyle.rebase(event.data.edit);
  }

  TestRunner.runTestSuite([
    function initFormattedStyle(next) {
      function callback(matchedResult) {
        if (!matchedResult) {
          TestRunner.addResult('empty styles');
          TestRunner.completeTest();
          return;
        }

        formattedStyle = matchedResult.nodeStyles()[1];
        next();
      }

      function nodeCallback(node) {
        TestRunner.cssModel.getMatchedStyles(node.id, false, false).then(callback);
      }
      ElementsTestRunner.selectNodeWithId('formatted', nodeCallback);
    },

    function testFormattedDisableLast(next) {
      formattedStyle.allProperties()[1].setDisabled(true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsertEnd(next) {
      formattedStyle.insertPropertyAt(2, 'endProperty', 'endValue', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedEnable(next) {
      formattedStyle.allProperties()[1].setDisabled(false).then(dumpFormattedAndCallNext.bind(null, next));
    },
  ]);

  // Data dumping

  function dumpFormattedAndCallNext(next, success) {
    if (!success) {
      TestRunner.addResult('error: operation failed.');
      TestRunner.completeTest();
      return;
    }

    dumpStyle(formattedStyle);
    if (next)
      next();
  }

  function dumpStyle(style) {
    if (!style)
      return;
    TestRunner.addResult('raw cssText:');
    TestRunner.addResult('{' + style.cssText + '}');
  }
})();
