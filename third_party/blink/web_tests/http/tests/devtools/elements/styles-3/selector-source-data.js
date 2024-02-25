// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that WebInspector.CSSStyleSheet methods work as expected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      /* c1 */
                               html
        #inspected.bar /* c2 */,
       /* c3 */ b
        /* c4 */  {
          text-decoration: none;
      }
      </style>
      <h1 id="inspected" class="bar">Inspect Me</h1>
    `);

  ElementsTestRunner.nodeWithId('inspected', nodeFound);

  async function nodeFound(node) {
    var response = await TestRunner.CSSAgent.invoke_getMatchedStylesForNode({nodeId: node.id});
    if (response.getError()) {
      TestRunner.addResult('Failed to get styles: ' + response.getError());
      return;
    }
    ElementsTestRunner.dumpRuleMatchesArray(response.matchedCSSRules);
    TestRunner.completeTest();
  }
})();
