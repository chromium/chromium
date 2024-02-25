// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates hasChildren flag upon adding children to collapsed nodes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
      <div id="container"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function appendChild()
      {
          var container = document.getElementById("container");
          var child = document.createElement("div");
          child.setAttribute("id", "appended");
          container.appendChild(child);
      }
  `);

  var containerNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        containerNode = node;
        TestRunner.addResult('========= Original ========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      ElementsTestRunner.selectNodeWithId('container', callback);
    },

    function testAppend(next) {
      function callback() {
        ElementsTestRunner.firstElementsTreeOutline().runPendingUpdates();
        TestRunner.deprecatedRunAfterPendingDispatches(function() {
          TestRunner.addResult('======== Appended =========');
          ElementsTestRunner.dumpElementsTree(containerNode);
          next();
        });
      }
      TestRunner.evaluateInPage('appendChild()', callback);
    }
  ]);
})();
