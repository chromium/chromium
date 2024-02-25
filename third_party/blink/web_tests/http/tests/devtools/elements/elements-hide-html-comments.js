// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Verifies show/hide HTML comments setting.\n`);
  await TestRunner.showPanel('elements');
  // Add the full html so that comments can be inserted between head and body
  await TestRunner.loadHTML(`
      <!-- comment 1 -->
      <html>
      <head>
      <base href="${TestRunner.url()}">
      </head>
      <!-- comment 2 -->
      <body>
      <p>
      <span id="inspect"></span>
      <!-- comment 3 -->
      </p>
      </body>
      </html>
    `);

  ElementsTestRunner.nodeWithId('inspect', onNode);

  function onNode(node) {
    var commentNode = node.nextSibling;
    ElementsTestRunner.selectNode(commentNode).then(onNodeSelected);
  }

  function onNodeSelected() {
    TestRunner.addResult('HTML comments shown:');
    ElementsTestRunner.dumpElementsTree();
    Common.Settings.settingForTest('show-html-comments').set(false);
    TestRunner.addResult('\nHTML comments hidden:');
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();
