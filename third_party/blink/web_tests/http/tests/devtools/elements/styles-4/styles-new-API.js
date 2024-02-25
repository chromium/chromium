// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that InspectorCSSAgent API methods work as expected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<head>
<link rel="stylesheet" href="resources/styles-new-API.css">
<style>

/* An inline stylesheet */
body.mainpage {
    text-decoration: none; /* at least one valid property is necessary for WebCore to match a rule */
    ;badproperty: 1badvalue1;
}

body.mainpage {
    prop1: val1;
    prop2: val2;
}

body:hover {
  color: #CDE;
}

#target:target {
  background: #bada55;
  outline: 5px solid lime;
}
</style>
</head>
<body id="mainBody" class="main1 main2 mainpage" style="font-weight: normal; width: 85%; background-image: url(bar.png)">
  <table width="50%" id="thetable">
  </table>
  <h1 id="toggle">H1</h1>
  <p id="target">:target</p>
</body>
    `);

  await (() => {
    // TODO(crbug.com/1046354): This is a workaround for loadHTML() resolving
    // before parsing is finished. In this case the link stylesheet is blocking
    // html parsing with BlockHTMLParsingOnStyleSheets enabled and mainBody is
    // not  found by selectNodeWithId below.
    let resolve;
    const promise = new Promise(r => resolve = r);
    TestRunner.waitForPageLoad(() => resolve());
    return promise;
  })();

  var bodyId;
  TestRunner.runTestSuite([
    function test_styles(next) {
      function callback(styles) {
        TestRunner.addResult('');
        TestRunner.addResult('=== Computed style property count for body ===');
        var propCount = styles.computedStyle.length;
        TestRunner.addResult(propCount > 200 ? 'OK' : 'FAIL (' + propCount + ')');

        TestRunner.addResult('');
        TestRunner.addResult('=== Matched rules for body ===');
        ElementsTestRunner.dumpRuleMatchesArray(styles.matchedCSSRules);

        TestRunner.addResult('');
        TestRunner.addResult('=== Pseudo rules for body ===');
        for (var i = 0; i < styles.pseudoElements.length; ++i) {
          TestRunner.addResult('PseudoType=' + styles.pseudoElements[i].pseudoType);
          ElementsTestRunner.dumpRuleMatchesArray(styles.pseudoElements[i].matches);
        }

        TestRunner.addResult('');
        TestRunner.addResult('=== Inherited styles for body ===');
        for (var i = 0; i < styles.inherited.length; ++i) {
          TestRunner.addResult('Level=' + (i + 1));
          ElementsTestRunner.dumpStyle(styles.inherited[i].inlineStyle);
          ElementsTestRunner.dumpRuleMatchesArray(styles.inherited[i].matchedCSSRules);
        }

        TestRunner.addResult('');
        TestRunner.addResult('=== Inline style for body ===');
        ElementsTestRunner.dumpStyle(styles.inlineStyle);
        next();
      }

      var resultStyles = {};

      function computedCallback(computedStyle) {
        if (!computedStyle) {
          TestRunner.addResult('Error');
          TestRunner.completeTest();
          return;
        }
        resultStyles.computedStyle = computedStyle;
      }

      function matchedCallback(response) {
        if (response.getError()) {
          TestRunner.addResult('error: ' + response.getError());
          TestRunner.completeTest();
          return;
        }

        resultStyles.inlineStyle = response.inlineStyle;
        resultStyles.matchedCSSRules = response.matchedCSSRules;
        resultStyles.pseudoElements = response.pseudoElements;
        resultStyles.inherited = response.inherited;
      }

      function nodeCallback(node) {
        bodyId = node.id;
        var promises = [
          TestRunner.CSSAgent.getComputedStyleForNode(node.id).then(computedCallback),
          TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id}).then(matchedCallback)
        ];
        Promise.all(promises).then(callback.bind(null, resultStyles));
      }
      ElementsTestRunner.selectNodeWithId('mainBody', nodeCallback);
    },

    async function test_forcedStateHover(next) {
      await TestRunner.CSSAgent.forcePseudoState(bodyId, ['hover']);
      const response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: bodyId});

      TestRunner.addResult('=== BODY with forced :hover ===');
      ElementsTestRunner.dumpRuleMatchesArray(response.matchedCSSRules);

      // Note: the forced :hover state persists for now, but is removed
      // as part of the next test.
      next();
    },

    async function test_forcedStateTarget(next) {
      await TestRunner.CSSAgent.forcePseudoState(bodyId, ['target']);
      ElementsTestRunner.nodeWithId('target', nodeCallback);

      async function nodeCallback(node) {
        const nodeId = node.id;
        await TestRunner.CSSAgent.forcePseudoState(nodeId, ['target']);
        const response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: nodeId});

        TestRunner.addResult('=== #target with forced :target ===');
        ElementsTestRunner.dumpRuleMatchesArray(response.matchedCSSRules);

        // Reset all forced pseudo states for the next tests.
        await TestRunner.CSSAgent.forcePseudoState(nodeId, []);
        next();
      }
    },

    function test_textNodeComputedStyles(next) {
      ElementsTestRunner.nodeWithId('toggle', nodeCallback);

      async function nodeCallback(node) {
        var textNode = node.children()[0];
        if (textNode.nodeType() !== Node.TEXT_NODE) {
          TestRunner.addResult('Failed to retrieve TextNode.');
          TestRunner.completeTest();
          return;
        }
        var computedStyle = await TestRunner.CSSAgent.getComputedStyleForNode(textNode.id);
        if (!computedStyle) {
          TestRunner.addResult('Error');
          return;
        }
        TestRunner.addResult('');
        TestRunner.addResult('=== Computed style property count for TextNode ===');
        var propCount = computedStyle.length;
        TestRunner.addResult(propCount > 200 ? 'OK' : 'FAIL (' + propCount + ')');
        next();
      }
    },

    function test_tableStyles(next) {
      async function nodeCallback(node) {
        var response = await TestRunner.CSSAgent.invoke_getInlineStylesForNode({nodeId: node.id});
        if (response.getError()) {
          TestRunner.addResult('error: ' + response.getError());
          next();
          return;
        }
        TestRunner.addResult('');
        TestRunner.addResult('=== Attributes style for table ===');
        ElementsTestRunner.dumpStyle(response.attributesStyle);

        var result = await TestRunner.CSSAgent.getStyleSheetText(response.inlineStyle.styleSheetId);

        TestRunner.addResult('');
        TestRunner.addResult('=== Stylesheet-for-inline-style text ===');
        TestRunner.addResult(result || '');

        await TestRunner.CSSAgent.setStyleSheetText(response.inlineStyle.styleSheetId, '');

        TestRunner.addResult('');
        TestRunner.addResult('=== Stylesheet-for-inline-style modification result ===');
        TestRunner.addResult(null);
        next();
      }
      ElementsTestRunner.nodeWithId('thetable', nodeCallback);
    },

    async function test_addRule(next) {
      var frameId = TestRunner.resourceTreeModel.mainFrame.id;
      var styleSheetId = await TestRunner.CSSAgent.createStyleSheet(frameId);
      if (!styleSheetId) {
        TestRunner.addResult('Error in CSS.createStyleSheet');
        next();
        return;
      }

      var range = {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0};

      var rule = await TestRunner.CSSAgent.addRule(styleSheetId, 'body {}', range);
      if (!rule) {
        TestRunner.addResult('Error in CSS.addRule');
        next();
        return;
      }

      var style = await TestRunner.CSSAgent.setStyleTexts([{
        styleSheetId: rule.style.styleSheetId,
        range: {
          startLine: rule.style.range.startLine,
          startColumn: rule.style.range.startColumn,
          endLine: rule.style.range.startLine,
          endColumn: rule.style.range.startColumn
        },
        text: 'font-family: serif;'
      }]);
      if (!style) {
        TestRunner.addResult('Error in CSS.setStyleTexts');
        next();
        return;
      }

      var response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: bodyId});
      if (response.getError()) {
        TestRunner.addResult('error: ' + response.getError());
        next();
        return;
      }

      TestRunner.addResult('');
      TestRunner.addResult('=== Matched rules after rule added ===');
      ElementsTestRunner.dumpRuleMatchesArray(response.matchedCSSRules);
      next();
    },
  ]);
})();
