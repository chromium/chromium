// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  'use strict';
  TestRunner.addResult(
      `Tests that modifying a rule in a stylesheet loaded from a different domain does not crash the renderer.\n`);
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);
  await TestRunner.addStylesheetTag('http://localhost:8000/devtools/elements/styles/resources/modify-cross-domain-rule.css');

  var nodeId;
  var nodeStyles;
  var rule;
  var matchedStyleResult;

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, onStyleSheetChanged, this);

  function onStyleSheetChanged(event) {
    if (!event.data || !event.data.edit)
      return;
    for (var style of matchedStyleResult.nodeStyles()) {
      if (style.parentRule)
        style.parentRule.rebase(event.data.edit);
      else
        style.rebase(event.data.edit);
    }
  }

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', selectCallback);

      function selectCallback() {
        for (const [id, node] of ElementsTestRunner.mappedNodes()) {
          if (node.getAttribute && node.getAttribute('id') === 'inspected') {
            nodeId = parseInt(id, 10);
            break;
          }
        }

        if (!nodeId) {
          TestRunner.completeTest();
          return;
        }

        TestRunner.cssModel.getMatchedStyles(nodeId, false, false).then(callback);
      }

      function callback(matchedResult) {
        if (!matchedResult) {
          TestRunner.addResult('[!] No rules found');
          TestRunner.completeTest();
          return;
        }

        nodeStyles = matchedResult.nodeStyles();
        matchedStyleResult = matchedResult;
        next();
      }
    },

    function testAddProperty(next) {
      for (var i = 0; i < nodeStyles.length; ++i) {
        var style = nodeStyles[i];
        if (style.parentRule && style.parentRule.isRegular()) {
          rule = style.parentRule;
          break;
        }
      }
      rule.style.appendProperty('width', '100%', callback);
      function callback(success) {
        TestRunner.addResult('=== Rule modified ===');
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
        return matchedStyleResult.recomputeMatchingSelectors(rule);
      }

      function successCallback() {
        TestRunner.addResult('=== Selector changed ===');
        TestRunner.addResult(rule.selectorText() + ' {' + rule.style.cssText + '}');
        TestRunner.addResult(
            'Selectors matching the (#inspected) node: ' +
            ElementsTestRunner.matchingSelectors(matchedStyleResult, rule));
        next();
      }
    }
  ]);
})();
