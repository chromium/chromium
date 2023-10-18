// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure upon setting attribute.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function setAttribute(name, value)
      {
          var node = document.getElementById("node");
          node.setAttribute(name, value);
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
      TestRunner.evaluateInPage('setAttribute(\'name\', \'value\')');
    },

    function testAttributeSameValueNotUpdated(next) {
      function callback() {
        TestRunner.addResult('===== On attribute modified (should be \'newValue\') =====');
        ElementsTestRunner.dumpElementsTree(targetNode);
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, callback);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, callback);
      // Setting the same property value should not result in the AttrModified event.
      TestRunner.evaluateInPage('setAttribute(\'name\', \'value\')');
      TestRunner.evaluateInPage('setAttribute(\'name\', \'value\')');
      TestRunner.evaluateInPage('setAttribute(\'name\', \'newValue\')');
    },

    function testAttributeRemoved(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
        TestRunner.addResult('=== On attribute removed ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
      TestRunner.evaluateInPage('removeAttribute(\'name\')');
    },

    function testSetAttributeValue(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, callback);
        TestRunner.addResult('=== Set attribute value ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, callback);
      targetNode.setAttributeValue('foo', 'bar');
    },

    function testSetAttributeText(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
        TestRunner.addResult('=== Set attribute as text ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
      targetNode.setAttribute('foo', 'foo2=\'baz2\' foo3=\'baz3\'');
    },

    function testRemoveAttributeAsText(next) {
      function callback() {
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
        TestRunner.addResult('=== Remove attribute as text ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrRemoved, callback);
      targetNode.setAttribute('foo3', '');
    },

    function testSetMalformedAttributeText(next) {
      function callback(error) {
        TestRunner.addResult('Error: ' + error);
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, callback);
        TestRunner.addResult('=== Set malformed attribute as text ===');
        ElementsTestRunner.dumpElementsTree(targetNode);
        next();
      }
      targetNode.setAttribute('foo2', 'foo2=\'missingquote', callback);
    }
  ]);
})();
