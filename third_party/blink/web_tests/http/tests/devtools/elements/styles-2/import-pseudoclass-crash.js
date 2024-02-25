// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that modifying stylesheet text with @import and :last-child selector does not crash (Bug 95324).\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <p id="lastchild">:last-child</p>
      </div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/import-pseudoclass-crash.css');

  ElementsTestRunner.nodeWithId('lastchild', nodeFound);

  function nodeFound(node) {
    TestRunner.cssModel.getMatchedStyles(node.id).then(matchedStylesCallback);
  }

  var styleSheetId;

  function matchedStylesCallback(matchedResult) {
    styleSheetId = matchedResult.nodeStyles()[1].styleSheetId;
    TestRunner.CSSAgent
        .setStyleSheetText(
            styleSheetId, '@import url("import-pseudoclass-crash-empty.css");\n\n:last-child { color: #000001; }\n')
        .then(modifiedCallback);
  }

  function modifiedCallback() {
    TestRunner.CSSAgent
        .setStyleSheetText(
            styleSheetId, '@import url("import-pseudoclass-crash-empty.css");\n\n:last-child { color: #002001; }\n')
        .then(modifiedCallback2);
  }

  function modifiedCallback2() {
    TestRunner.completeTest();
  }
})();
