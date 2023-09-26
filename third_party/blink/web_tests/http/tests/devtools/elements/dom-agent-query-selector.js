// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests DOMAgent.querySelector and DOMAgent.querySelectorAll.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="id1" class="foo"></div>
      <div id="id2" class="foo"></div>

      <div id="container">
          <div id="id3" class="foo"></div>
          <div id="id4" class="foo"></div>
          <div id="id5" class="foo"></div>
          <div id="id6" class="foo"></div>
      </div>
    `);

  ElementsTestRunner.selectNodeWithId('container', step1);

  function step1(node) {
    var containerId = node.id;
    var documentId = node.ownerDocument.id;

    TestRunner.runTestSuite([
      function testDocumentQuerySelector(next) {
        TestRunner.DOMAgent.querySelector(documentId, 'div.foo').then(dumpNodes.bind(null, next));
      },

      function testDocumentQuerySelectorAll(next) {
        TestRunner.DOMAgent.querySelectorAll(documentId, 'div.foo').then(dumpNodes.bind(null, next));
      },

      function testQuerySelector(next) {
        TestRunner.DOMAgent.querySelector(containerId, 'div.foo').then(dumpNodes.bind(null, next));
      },

      function testQuerySelectorAll(next) {
        TestRunner.DOMAgent.querySelectorAll(containerId, 'div.foo').then(dumpNodes.bind(null, next));
      }
    ]);
  }

  function dumpNodes(next, nodeIds) {
    if (!nodeIds) {
      next();
      return;
    }

    if (!(nodeIds.constructor && nodeIds.constructor.name === 'Array'))
      nodeIds = [nodeIds];

    for (var i = 0; i < nodeIds.length; ++i) {
      if (!nodeIds[i])
        TestRunner.addResult('no results');
      else {
        var node = TestRunner.domModel.nodeForId(nodeIds[i]);
        TestRunner.addResult('node id: ' + node.getAttribute('id'));
      }
    }
    next();
  }
})();
