// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that CSS variables are resolved properly for DOM inheritance`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      span {
        --color: blue;
      }

      div {
        color: var(--color);
      }

      body {
        --color: red;
      }
    </style>
    <div>
      <span id=inspected></span>
    </div>
  `);

  const node = await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  const matchedStyles = await TestRunner.cssModel.getMatchedStyles(node.id);
  for (const style of matchedStyles.nodeStyles()) {
    const selector = style.parentRule ? style.parentRule.selectorText() : 'element.style';
    const value = 'var(--color)';
    TestRunner.addResult(`compute "${value}" for ${selector}: ` + matchedStyles.computeValue(style, value));
  }
  TestRunner.completeTest();
})();
