// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that WebInspector.CSSStyleSheet methods work as expected.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      /* An inline stylesheet */
      body.mainpage {
          text-decoration: none;
      }
      </style>
      <h1 id="inspected">Inspect Me</h1>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/get-set-stylesheet-text.css');

  var foundStyleSheetHeader;
  var foundStyleSheetText;

  findStyleSheet();

  function findStyleSheet() {
    var styleSheetHeaders = TestRunner.cssModel.styleSheetHeaders();
    for (var i = 0; i < styleSheetHeaders.length; ++i) {
      styleSheetHeader = styleSheetHeaders[i];
      if (styleSheetHeader.sourceURL.indexOf('get-set-stylesheet-text.css') >= 0) {
        foundStyleSheetHeader = styleSheetHeader;
        foundStyleSheetHeader.requestContent().then(callback);
      }
      if (!foundStyleSheetHeader)
        TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, styleSheetAdded);
    }

    function callback({ content, error, isEncoded }) {
      foundStyleSheetText = content;
      TestRunner.runTestSuite([testSetText, testNewElementStyles]);
    }

    function styleSheetAdded() {
      TestRunner.cssModel.removeEventListener(SDK.CSSModel.Events.StyleSheetAdded, styleSheetAdded);
      findStyleSheet();
    }
  }

  function testSetText(next) {
    function callback(error) {
      if (error) {
        TestRunner.addResult('Failed to set stylesheet text: ' + error);
        return;
      }
    }

    TestRunner.addResult('=== Original stylesheet text: ===');
    TestRunner.addResult(foundStyleSheetText);
    TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, next, this);
    TestRunner.cssModel.setStyleSheetText(foundStyleSheetHeader.id, 'h1 { COLOR: Red; }', true).then(callback);
  }

  function testNewElementStyles() {
    function callback(response) {
      if (response[Protocol.Error]) {
        TestRunner.addResult('error: ' + response[Protocol.Error]);
        return;
      }

      TestRunner.addResult('=== Matched rules for h1 after setText() ===');
      dumpMatchesArray(response.matchedCSSRules);
      TestRunner.completeTest();
    }

    function nodeCallback(node) {
      TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id}).then(callback);
    }

    ElementsTestRunner.selectNodeWithId('inspected', nodeCallback);
  }


  // Data dumping

  function dumpMatchesArray(rules) {
    if (!rules)
      return;
    for (var i = 0; i < rules.length; ++i)
      dumpRuleOrStyle(rules[i].rule);
  }

  function dumpRuleOrStyle(ruleOrStyle) {
    if (!ruleOrStyle)
      return;
    var isRule = !!(ruleOrStyle.style);
    var style = isRule ? ruleOrStyle.style : ruleOrStyle;
    TestRunner.addResult('');
    TestRunner.addResult(isRule ? 'rule' : 'style');
    TestRunner.addResult((isRule ? (ruleOrStyle.selectorList.text + ': [' + ruleOrStyle.origin + ']') : 'raw style'));
    ElementsTestRunner.dumpStyle(style);
  }
})();
