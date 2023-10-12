// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`This test verifies that template's content DocumentFragment is accessible from DevTools.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <template id="tpl">
          <div>Hello!</div>
      </template>
    `);

  ElementsTestRunner.expandElementsTree(function() {
    var contentNode = ElementsTestRunner.expandedNodeWithId('tpl').templateContent();
    Elements.ElementsPanel.ElementsPanel.instance().selectDOMNode(contentNode, true);
    ConsoleTestRunner.evaluateInConsole('$0', callback);
  });

  function callback(result) {
    TestRunner.addResult('SUCCESS');
    TestRunner.completeTest();
  }
})();
