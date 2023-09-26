// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests DOMAgent.setOuterHTML invoked on body tag. See https://bugs.webkit.org/show_bug.cgi?id=62272. \n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe src="../resources/set-outer-html-body-iframe.html" onload="runTest()"></iframe>
    `);

  var htmlNode;
  var bodyNode;
  var headNode;

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(step1);
      function step1() {
        htmlNode = ElementsTestRunner.expandedNodeWithId('html');
        headNode = ElementsTestRunner.expandedNodeWithId('head');
        bodyNode = ElementsTestRunner.expandedNodeWithId('body');
        next();
      }
    },

    function testSetBody(next) {
      TestRunner.DOMAgent.setOuterHTML(bodyNode.id, '<body>New body content</body>').then(dumpHTML(next));
    },

    function testInsertComments(next) {
      TestRunner.DOMAgent
          .setOuterHTML(bodyNode.id, '<!-- new comment between head and body --><body>New body content</body>')
          .then(dumpHTML(next));
    },

    function testSetHead(next) {
      TestRunner.DOMAgent.setOuterHTML(headNode.id, '<head><!-- new head content --></head>').then(dumpHTML(next));
    },

    function testSetHTML(next) {
      TestRunner.DOMAgent
          .setOuterHTML(
              htmlNode.id,
              '<html><head><!-- new head content --></head><body>Setting body as a part of HTML.</body></html>')
          .then(dumpHTML(next));
    }
  ]);

  function dumpHTML(next) {
    async function dump() {
      var text = await TestRunner.DOMAgent.getOuterHTML(htmlNode.id);
      TestRunner.addResult(text);
      next();
    }
    return dump;
  }
})();
