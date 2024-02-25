// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test confirms that updating the shadow dom is reflected to the Inspector.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <div id="container">
          <div id="host"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      var shadowRoot = host.attachShadow({mode: 'open'});
      shadowRoot.innerHTML = "<div></div>";
    `);
  await TestRunner.evaluateInPagePromise(`
      function updateShadowDOM()
      {
          shadowRoot.removeChild(shadowRoot.firstChild);
      }
  `);

  ElementsTestRunner.expandElementsTree(function() {
    TestRunner.evaluateInPage('updateShadowDOM()', function() {
      ElementsTestRunner.expandElementsTree(function() {
        var containerElem = ElementsTestRunner.expandedNodeWithId('container');
        ElementsTestRunner.dumpElementsTree(containerElem);
        TestRunner.completeTest();
      });
    });
  });
})();
