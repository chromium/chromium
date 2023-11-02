// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that CSSPropertyPrompt properly builds suggestions.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
      body {
        --red-color: red;
        --blue-color: blue;
        --other: 12px;
      }
    </style>
    <div id="inner" style="color: red"></div>
  `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inner');
  const treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
  const valuePrompt = new Elements.StylesSidebarPane.CSSPropertyPrompt(treeElement, false /* isEditingName */);
  const results = await valuePrompt.buildPropertyCompletions('var(', '--', true /* true */)
  for (const result of results) {
    TestRunner.addResult(result.title)
    TestRunner.addResult('  text: ' + result.text)
    TestRunner.addResult('  priority: ' + result.priority)
    TestRunner.addResult('  subtitle: ' + result.subtitle)
    TestRunner.addResult('  subtitleRenderer: ' + result.subtitleRenderer)
  }
  TestRunner.completeTest();
})();
