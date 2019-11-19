// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that CSS variables are defined correctly wrt DOM inheritance`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      body {
        --body-variable: red;
      }

      div {
        --div-variable: blue;
      }

      .myelement {
        --another-div-variable: grey;
      }

      span {
        --span-variable: green;
        --camelCased: blue;
      }
    </style>
    <body>
      <div class=myelement>
        <span id=inspected></span>
      </div>
    </body>
  `);


  const node = await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  const matchedStyles = await TestRunner.cssModel.matchedStylesPromise(node.id);
  TestRunner.addResult('matchedStyles.availableCSSVariables()');
  const styles = matchedStyles.nodeStyles();
  dumpCSSVariables(styles[0]);
  dumpCSSVariables(styles[1]);
  dumpCSSVariables(styles[2]);
  dumpCSSVariables(styles[3]);
  TestRunner.completeTest();

  function dumpCSSVariables(style) {
    const rule = style.parentRule;
    TestRunner.addResult(rule ? rule.selectorText() : 'element.style');
    const cssVariables = matchedStyles.availableCSSVariables(style);
    for (const cssVar of cssVariables)
      TestRunner.addResult('  ' + cssVar);
  }
})();
