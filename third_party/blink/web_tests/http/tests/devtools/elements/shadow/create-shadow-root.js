// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure upon shadow root creation.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container"><div id="child"></div></div>
      <div id="containerOpen"><div id="childOpen"></div></div>
      <div id="containerClosed"><div id="childClosed"></div></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function createShadowRoot(id)
      {
          var container = document.getElementById("container");
          var root = container.attachShadow({mode: 'open'});
          root.innerHTML = "<div id='" + id + "'></div>";
      }

      function createShadowRootV1(containerId, id, mode)
      {
          var container = document.getElementById(containerId);
          var root = container.attachShadow({ mode: mode });
          root.innerHTML = "<div id='" + id + "'></div>";
      }
  `);

  TestRunner.runTestSuite([
    function testCreateShadowRoot(next) {
      testShadowRoot('container', 'createShadowRoot(\'shadow-1\')', next);
    },

    function testCreateOpenShadowRoot(next) {
      testShadowRoot('containerOpen', 'createShadowRootV1(\'containerOpen\', \'shadow-3\', \'open\')', next);
    },

    function testCreateCloseShadowRoot(next) {
      testShadowRoot('containerClosed', 'createShadowRootV1(\'containerClosed\', \'shadow-4\', \'closed\')', next);
    },
  ]);

  function testShadowRoot(containerId, code, next) {
    var containerNode;
    ElementsTestRunner.expandElementsTree(dumpBefore);

    function dumpBefore() {
      containerNode = ElementsTestRunner.expandedNodeWithId(containerId);
      TestRunner.addResult('==== before ====');
      ElementsTestRunner.dumpElementsTree(containerNode);
      TestRunner.evaluateInPage(code, ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, dumpAfter));
    }

    function dumpAfter() {
      TestRunner.addResult('==== after ====');
      ElementsTestRunner.dumpElementsTree(containerNode);
      next();
    }
  }
})();
