// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that proper data and start/end offset positions are reported for CSS style declarations and properties.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<head>
<link rel="stylesheet" href="../styles/resources/styles-source-offsets.css">
<style>

body.mainpage {
    text-decoration: none; /* at least one valid property is necessary for WebCore to match a rule */
    badproperty: 1badvalue1;
}

</style>
</head>
<body id="mainBody" class="main1 main2 mainpage" style="font-weight: normal; width: 80%">
</body>
    `);

  function dumpStyleData(ruleOrStyle) {
    var isRule = !!(ruleOrStyle.style);
    var style;
    var header = '';
    if (isRule) {
      if (ruleOrStyle.origin !== 'regular')
        return;
      style = ruleOrStyle.style;
      var selectorRanges = [];
      var selectors = ruleOrStyle.selectorList.selectors;
      var firstRange = selectors[0].range;
      var lastRange = selectors[selectors.length - 1].range;
      var range = {
        startLine: firstRange.startLine,
        startColumn: firstRange.startColumn,
        endLine: lastRange.endLine,
        endColumn: lastRange.endColumn
      };
      header = ruleOrStyle.selectorList.text + ': ' + (range ? ElementsTestRunner.rangeText(range) : '');
    } else {
      style = ruleOrStyle;
      header = 'element.style:';
    }
    TestRunner.addResult(header + ' ' + ElementsTestRunner.rangeText(style.range));
    var allProperties = style.cssProperties;
    for (var i = 0; i < allProperties.length; ++i) {
      var property = allProperties[i];
      if (!property.range)
        continue;
      TestRunner.addResult(
          '[\'' + property.name + '\':\'' + property.value + '\'' + (property.important ? ' !important' : '') +
          (('parsedOk' in property) ? ' non-parsed' : '') + '] @' + ElementsTestRunner.rangeText(property.range));
    }
  }

  await (() => {
    // TODO(crbug.com/1046354): This is a workaround for loadHTML() resolving
    // before parsing is finished. In this case the stylesheet is blocking html
    // parsing with BlockHTMLParsingOnStyleSheets enabled and mainBody is not
    // found by selectNodeWithId below.
    let resolve;
    const promise = new Promise(r => resolve = r);
    TestRunner.waitForPageLoad(() => resolve());
    return promise;
  })();

  ElementsTestRunner.selectNodeWithId('mainBody', step1);

  async function step1(node) {
    var response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id});

    for (var rule of response.matchedCSSRules)
      dumpStyleData(rule.rule);
    dumpStyleData(response.inlineStyle);
    TestRunner.completeTest();
  }
})();
