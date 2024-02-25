// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that InspectorCSSAgent formats the CSS style text based on the CSS model modifications.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      #formatted {
          /* leading comment */
          color: red;   /* comment1 */
          zoom: 1;/* comment2 */ /* like: property */
          padding: 0
      }

      #unformatted {/*leading comment*/color:red;zoom:1;padding:0;}

      </style>
      <div id="formatted">Formatted</div>
      <div id="unformatted">Unformatted</div>
    `);

  var formattedStyle;
  var unformattedStyle;


  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, onStyleSheetChanged, this);

  function onStyleSheetChanged(event) {
    if (!event.data || !event.data.edit)
      return;
    if (formattedStyle)
      formattedStyle.rebase(event.data.edit);
    if (unformattedStyle)
      unformattedStyle.rebase(event.data.edit);
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
        TestRunner.cssModel.getMatchedStyles(node.id).then(callback);
      }
      ElementsTestRunner.selectNodeWithId('formatted', nodeCallback);
    },

    function testFormattedInsertStart(next) {
      formattedStyle.insertPropertyAt(
          0, 'firstProperty', 'rgba(1, 2, 3, 0)', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedRemoveStart(next) {
      formattedStyle.allProperties()[0].setText('', true, true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsertMiddle(next) {
      formattedStyle.insertPropertyAt(
          1, 'middleProperty', 'middleValue /* comment */', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedRemoveMiddle(next) {
      formattedStyle.allProperties()[1].setText('', true, true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsertEnd(next) {
      formattedStyle.insertPropertyAt(3, 'endProperty', 'endValue', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedRemoveEnd(next) {
      formattedStyle.allProperties()[3].setText('', true, true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedDisableStart(next) {
      formattedStyle.allProperties()[0].setDisabled(true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedDisableEnd(next) {
      formattedStyle.allProperties()[2].setDisabled(true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedDisableMiddle(next) {
      formattedStyle.allProperties()[1].setDisabled(true).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsert1(next) {
      formattedStyle.insertPropertyAt(0, 'propA', 'valA', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsert2(next) {
      formattedStyle.insertPropertyAt(2, 'propB', 'valB', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedInsert3(next) {
      formattedStyle.insertPropertyAt(5, 'propC', 'valC', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedEnableStart(next) {
      formattedStyle.allProperties()[1].setDisabled(false).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedEnableEnd(next) {
      formattedStyle.allProperties()[4].setDisabled(false).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedEnableMiddle(next) {
      formattedStyle.allProperties()[3].setDisabled(false).then(dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedWithMeta(next) {
      formattedStyle.insertPropertyAt(0, '-webkit-animation', 'linear', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedWithMetaValue(next) {
      formattedStyle.insertPropertyAt(1, 'unicode-bidi', 'webkit-isolate', dumpFormattedAndCallNext.bind(null, next));
    },

    function testFormattedWithAtoms(next) {
      formattedStyle.insertPropertyAt(
          0, 'border-left', '1px solid rgb(1,1,1)', dumpFormattedAndCallNext.bind(null, next));
    },

    function initUnformattedStyle(next) {
      function callback(matchedResult) {
        if (!matchedResult) {
          TestRunner.addResult('empty styles');
          TestRunner.completeTest();
          return;
        }

        unformattedStyle = matchedResult.nodeStyles()[1];
        next();
      }

      function nodeCallback(node) {
        TestRunner.cssModel.getMatchedStyles(node.id).then(callback);
      }
      ElementsTestRunner.selectNodeWithId('unformatted', nodeCallback);
    },

    function testUnformattedInsertStart(next) {
      unformattedStyle.insertPropertyAt(0, 'firstProperty', 'firstValue', dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedRemoveStart(next) {
      unformattedStyle.allProperties()[0].setText('', true, true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedInsertMiddle(next) {
      unformattedStyle.insertPropertyAt(
          1, 'middleProperty', 'middleValue', dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedRemoveMiddle(next) {
      unformattedStyle.allProperties()[1].setText('', true, true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedInsertEnd(next) {
      unformattedStyle.insertPropertyAt(3, 'endProperty', 'endValue', dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedRemoveEnd(next) {
      unformattedStyle.allProperties()[3].setText('', true, true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedDisableStart(next) {
      unformattedStyle.allProperties()[0].setDisabled(true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedDisableEnd(next) {
      unformattedStyle.allProperties()[2].setDisabled(true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedDisableMiddle(next) {
      unformattedStyle.allProperties()[1].setDisabled(true).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedEnableStart(next) {
      unformattedStyle.allProperties()[0].setDisabled(false).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedEnableEnd(next) {
      unformattedStyle.allProperties()[2].setDisabled(false).then(dumpUnformattedAndCallNext.bind(null, next));
    },

    function testUnformattedEnableMiddle(next) {
      unformattedStyle.allProperties()[1].setDisabled(false).then(dumpUnformattedAndCallNext.bind(null, next));
    }
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

  function dumpUnformattedAndCallNext(next, success) {
    if (!success) {
      TestRunner.addResult('error: operation failed.');
      TestRunner.completeTest();
      return;
    }

    dumpStyle(unformattedStyle);
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
