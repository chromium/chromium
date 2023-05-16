// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verify that CSS variables are resolved inside cascade`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      .foo {
        --foo: active-foo;
        --baz: active-baz !important;
        --baz: passive-baz;
      }

      div {
        --dark: darkgrey;
        --light: lightgrey;
        --theme: var(--dark);
        --shadow: 1px var(--theme);
        text-shadow: 2px var(--shadow);
        --cycle-a: var(--cycle-b);
        --cycle-b: var(--cycle-c);
        --cycle-c: var(--cycle-a);
      }

      * {
        --foo: passive-foo;
        --width: 1px;
      }
    </style>
    <div>
      <div id=inspected class=foo></div>
    </div>
  `);


  const node = await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  const matchedStyles = await TestRunner.cssModel.getMatchedStyles(node.id);
  TestRunner.addResult('matchedStyles.computeCSSVariable()');
  dumpCSSVariable('--foo');
  dumpCSSVariable('--baz');
  dumpCSSVariable('--does-not-exist');
  dumpCSSVariable('--dark');
  dumpCSSVariable('--light');
  dumpCSSVariable('--theme');
  dumpCSSVariable('--shadow');
  dumpCSSVariable('--width');
  dumpCSSVariable('--cycle-a');
  dumpCSSVariable('--cycle-b');
  dumpCSSVariable('--cycle-c');
  TestRunner.addResult('matchedStyles.computeValue()');
  dumpValue('1px var(--dark) 2px var(--theme)');
  dumpValue('1px var(--theme)');
  dumpValue('rgb(100, 200, 300) var(--some-color, blue    ) 1px');
  dumpValue('var(--not-existing)');
  dumpValue('var(--not-existing-with-default, red)');
  dumpValue('var(--width)solid black');
  TestRunner.completeTest();

  function dumpCSSVariable(varName) {
    const style = matchedStyles.nodeStyles()[0];
    TestRunner.addResult('  ' + varName + ' === ' + matchedStyles.computeCSSVariable(style, varName));
  }

  function dumpValue(value) {
    const style = matchedStyles.nodeStyles()[0];
    TestRunner.addResult('  ' + value + ' === ' + matchedStyles.computeValue(style, value));
  }
})();
