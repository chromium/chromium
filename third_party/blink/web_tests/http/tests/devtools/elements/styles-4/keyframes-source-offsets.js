// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that proper data and start/end offset positions are reported for CSS keyframes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @keyframes animName {
          from, 20% {
              height: 200px;
              color: red;
          }
          to {
              height: 500px;
          }
      }
      </style>
      <div id="element"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/keyframes.css');

  function dumpRule(rule) {
    TestRunner.addResult(
        '\n' + rule.animationName.text + ' @' + ElementsTestRunner.rangeText(rule.animationName.range));
    for (var keyframe of rule.keyframes) {
      TestRunner.addResult(
          keyframe.keyText.text + ' @' + ElementsTestRunner.rangeText(keyframe.keyText.range) + ' {} @' +
          ElementsTestRunner.rangeText(keyframe.style.range));
      var allProperties = keyframe.style.cssProperties;
      for (var i = 0; i < allProperties.length; ++i) {
        var property = allProperties[i];
        if (!property.range)
          continue;
        TestRunner.addResult(
            property.name + ':' + property.value + (property.important ? ' !important' : '') +
            (('parsedOk' in property) ? ' non-parsed' : '') + ' @' + ElementsTestRunner.rangeText(property.range));
      }
    }
  }

  ElementsTestRunner.selectNodeWithId('element', step1);

  async function step1(node) {
    var response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id});

    for (var animation of response.cssKeyframesRules)
      dumpRule(animation);

    TestRunner.addResult('\n>> Modifying keyframe rule');
    var style = new SDK.CSSStyleDeclaration.CSSStyleDeclaration(
        TestRunner.cssModel, null, response.cssKeyframesRules[1].keyframes[0].style,
        SDK.CSSStyleDeclaration.Type.Regular);
    await style.setText('width: 123px');

    response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id});

    for (var animation of response.cssKeyframesRules)
      dumpRule(animation);
    TestRunner.completeTest();
  }
})();
