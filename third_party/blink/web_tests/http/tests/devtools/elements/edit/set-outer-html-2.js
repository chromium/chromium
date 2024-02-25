// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests DOMAgent.setOuterHTML protocol method (part 2).\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="display:none">
      <p>WebKit is used by <a href="http://www.apple.com/safari/">Safari</a>, Dashboard, etc..</p>
      <h2>Getting involved</h2>
      <p id="identity">There are many ways to get involved. You can:</p>
      <ul>
         <li></li>
      </ul>
      <ul>
         <li></li>
      </ul>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      document.getElementById("identity").wrapperIdentity = "identity";
  `);

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.setUpTestSuite(next);
    },

    function testChangeMultipleThings(next) {
      var text = ElementsTestRunner.containerText.replace(/<li>.*<\/li>/, '');
      text = text.replace('<h2>', '<h2 foo="bar" bar="baz">');
      ElementsTestRunner.setOuterHTML(text, next);
    },

    function testChangeNestingLevel(next) {
      var text = ElementsTestRunner.containerText.replace('<ul>', '<div><ul>');
      var text = text.replace('</ul>', '</ul></div>');
      ElementsTestRunner.setOuterHTML(text, next);
    },

    function testSwapNodes(next) {
      var text = ElementsTestRunner.containerText.replace('<h2>Getting involved</h2>', '');
      var text = text.replace('</div>', '<h2>Getting involved</h2></div>');
      ElementsTestRunner.setOuterHTML(text, next);
    },

    function testEditTwoRoots(next) {
      var text = ElementsTestRunner.containerText + '<div>Additional node</div>';
      ElementsTestRunner.setOuterHTML(text, next);
    },

    function testDupeNode(next) {
      ElementsTestRunner.patchOuterHTML(
          '<h2>Getting involved</h2>', '<h2>Getting involved</h2><h2>Getting involved</h2>', next);
    }
  ]);
})();
