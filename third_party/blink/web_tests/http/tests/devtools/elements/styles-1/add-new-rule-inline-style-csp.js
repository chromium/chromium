// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that adding a new rule does not crash the renderer and modifying an inline style does not report errors when forbidden by Content-Security-Policy.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);

  var nodeId;
  var rule;
  var matchedStyles;

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function testAddRule(next) {
      ElementsTestRunner.nodeWithId('inspected', nodeCallback);

      function nodeCallback(node) {
        nodeId = node.id;
        ElementsTestRunner.addNewRule('#inspected', successCallback);
      }

      function successCallback(section) {
        rule = section.style().parentRule;
        matchedStyles = section.matchedStyles;
        TestRunner.addResult('=== Rule added ===');
        TestRunner.addResult(rule.selectorText() + ' {' + rule.style.cssText + '}');
        TestRunner.addResult(
            'Selectors matching the (#inspected) node: ' + ElementsTestRunner.matchingSelectors(matchedStyles, rule));
        next();
      }
    },

    function testAddProperty(next) {
      rule.style.appendProperty('width', '100%', callback);

      function callback(success) {
        TestRunner.addResult('=== Added rule modified ===');
        if (!success) {
          TestRunner.addResult('[!] No valid rule style received');
          TestRunner.completeTest();
        } else {
          ElementsTestRunner.dumpCSSStyleDeclaration(rule.style);
          rule.setSelectorText('body').then(onSelectorUpdated).then(successCallback);
        }
      }

      function onSelectorUpdated(success) {
        if (!success) {
          TestRunner.addResult('[!] Failed to change selector');
          TestRunner.completeTest();
          return;
        }
        return matchedStyles.recomputeMatchingSelectors(rule);
      }

      function successCallback() {
        TestRunner.addResult('=== Selector changed ===');
        TestRunner.addResult(rule.selectorText() + ' {' + rule.style.cssText + '}');
        TestRunner.addResult(
            'Selectors matching the (#inspected) node: ' + ElementsTestRunner.matchingSelectors(matchedStyles, rule));

        next();
      }
    },

    function testModifyInlineStyle(next) {
      var inlineStyle;
      TestRunner.cssModel.getInlineStyles(nodeId).then(stylesCallback);
      TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, onStyleSheetChanged);
      function onStyleSheetChanged(event) {
        if (event.data && event.data.edit)
          inlineStyle.rebase(event.data.edit);
      }

      function stylesCallback(inlineStyleResult) {
        if (!inlineStyleResult || !inlineStyleResult.inlineStyle) {
          TestRunner.completeTest();
          return;
        }
        inlineStyle = inlineStyleResult.inlineStyle;
        inlineStyle.appendProperty('font-size', '14px', appendCallback);
      }

      function appendCallback(success) {
        TestRunner.addResult('=== Inline style modified ===');
        if (!success) {
          TestRunner.addResult('No valid inline style received');
          TestRunner.completeTest();
          return;
        }

        ElementsTestRunner.dumpCSSStyleDeclaration(inlineStyle);
        next();
      }
    }
  ]);
})();
