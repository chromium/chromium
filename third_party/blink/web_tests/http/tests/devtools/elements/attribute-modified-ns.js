// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that elements panel updates dom tree structure upon changing the attribute with namespace.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <svg>
          <a id="node" xlink:href="http://localhost">link</a>
      </svg>
    `);
  await TestRunner.evaluateInPagePromise(`
      function setAttribute(namespace, name, value)
      {
          var node = document.getElementById("node");
          node.setAttributeNS(namespace, name, value);
      }

      function removeAttribute(name)
      {
          var node = document.getElementById("node");
          node.removeAttribute(name);
      }
  `);

  var targetNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        targetNode = node;
        TestRunner.addResult('========= Original ========');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      ElementsTestRunner.selectNodeWithId('node', callback);
    },

    function testAttributeUpdated(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, callback);
        TestRunner.addResult('===== On attribute set =====');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, callback);
      TestRunner.evaluateInPage('setAttribute(\'http://www.w3.org/1999/xlink\', \'xlink:href\', \'changed-url\')');
    },

    function testAttributeRemoved(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
        TestRunner.addResult('=== On attribute removed ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
      TestRunner.evaluateInPage('removeAttribute(\'xlink:href\')');
    },
  ]);
})();
