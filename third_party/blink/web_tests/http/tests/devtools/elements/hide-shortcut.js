// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';

(async function() {
  TestRunner.addResult(
      `Tests the hide shortcut, which toggles visibility:hidden on the node and it's ancestors. Bug 110641\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #parent-node::before {
          content: "before";
      }

      #parent-node::after {
          content: "after";
      }
      </style>
      <p>
      Tests the hide shortcut, which toggles visibility:hidden on the node and it's ancestors.
      <a href="https://bugs.webkit.org/show_bug.cgi?id=110641">Bug 110641</a>
      </p>

      <div id="parent-node">parent
          <div style="visibility:hidden">hidden
              <div id="child-node" style="visibility:visible">child</div>
          </div>
      </div>

      <iframe src="resources/hide-shortcut-iframe.html" onload="runTest()">

      </body>
      </html>
      </iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function pseudoVisibility(pseudo)
      {
          var parentNode = document.getElementById("parent-node");
          return getComputedStyle(parentNode, ":" + pseudo).visibility;
      }

      function pseudoIframeVisibility()
      {
          var parentNode = frames[0].document.getElementById("frame-node");
          return getComputedStyle(parentNode).visibility;
      }
  `);

  var treeOutline;
  var parentNode;
  var childNode;
  var frameNode;

  TestRunner.runTestSuite([
    function testSetUp(next) {
      treeOutline = ElementsTestRunner.firstElementsTreeOutline();

      ElementsTestRunner.nodeWithId('parent-node', callback);

      function callback(node) {
        parentNode = node;
        ElementsTestRunner.nodeWithId('child-node', callback2);
      }

      function callback2(node) {
        childNode = node;
        ElementsTestRunner.nodeWithId('frame-node', callback3);
      }

      function callback3(node) {
        frameNode = node;
        next();
      }
    },

    function testToggleHideShortcutOn(next) {
      treeOutline.toggleHideElement(parentNode).then(callback);

      function callback() {
        TestRunner.addResult('=== Added hide shortcut ===');
        TestRunner.cssModel.getComputedStyle(parentNode.id).then(callback2);
      }

      function callback2(style) {
        TestRunner.addResult('=== Parent node is hidden ===');
        TestRunner.addResult(getPropertyText(style, 'visibility'));
        TestRunner.cssModel.getComputedStyle(childNode.id).then(callback3);
      }

      function callback3(style) {
        TestRunner.addResult('=== Child node is hidden ===');
        TestRunner.addResult(getPropertyText(style, 'visibility'));
        next();
      }
    },

    function testToggleHideShortcutOff(next) {
      treeOutline.toggleHideElement(parentNode).then(callback);

      function callback() {
        TestRunner.addResult('=== Removed hide shortcut ===');
        TestRunner.cssModel.getComputedStyle(parentNode.id).then(callback2);
      }

      function callback2(style) {
        TestRunner.addResult('=== Parent node is visible ===');
        TestRunner.addResult(getPropertyText(style, 'visibility'));
        TestRunner.cssModel.getComputedStyle(childNode.id).then(callback3);
      }

      function callback3(style) {
        TestRunner.addResult('=== Child node is visible ===');
        TestRunner.addResult(getPropertyText(style, 'visibility'));
        next();
      }
    },

    function testToggleHideBeforePseudoShortcutOn(next) {
      testPseudoToggle(parentNode.beforePseudoElement(), next);
    },

    function testToggleHideAfterPseudoShortcutOn(next) {
      testPseudoToggle(parentNode.afterPseudoElement(), next);
    },

    function testToggleHideBeforePseudoShortcutOff(next) {
      testPseudoToggle(parentNode.beforePseudoElement(), next);
    },

    function testToggleHideAfterPseudoShortcutOff(next) {
      testPseudoToggle(parentNode.afterPseudoElement(), next);
    },

    function testToggleHideShortcutOnInFrame(next) {
      treeOutline.toggleHideElement(frameNode).then(callback);

      function callback() {
        TestRunner.evaluateInPagePromise('pseudoIframeVisibility()').then(function(result) {
          TestRunner.addResult('=== Added hide shortcut in frame ===');
          TestRunner.addResult('=== Frame node is hidden ===');
          TestRunner.addResult('visibility: ' + result + ';');
          next();
        });
      }
    }
  ]);

  function getPropertyText(computedStyle, propertyName) {
    return Platform.StringUtilities.sprintf('%s: %s;', propertyName, computedStyle.get(propertyName));
  }

  function testPseudoToggle(pseudoNode, next) {
    treeOutline.toggleHideElement(pseudoNode).then(callback);
    function callback() {
      var pseudoNodeTypeArg = pseudoNode.pseudoType() ? ('"' + pseudoNode.pseudoType() + '"') : 'undefined';
      TestRunner.evaluateInPagePromise('pseudoVisibility(' + pseudoNodeTypeArg + ')').then(function(result) {
        TestRunner.addResult('::' + pseudoNode.pseudoType() + ' node visibility: \'' + result + '\'');
        next();
      });
    }
  }
})();
