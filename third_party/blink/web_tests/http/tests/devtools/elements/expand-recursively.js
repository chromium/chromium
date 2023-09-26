// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that expanding elements recursively works.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="depth-1">
          <div id="depth-2">
              <div id="depth-3">
                  <div id="depth-4">
                      <div id="depth-5">
                          <div id="depth-6">
                              <div id="depth-7">
                                  <div id="depth-8">
                                      <div id="depth-9">
                                          <div id="depth-10"></div>
                                      </div>
                                  </div>
                              </div>
                          </div>
                      </div>
                  </div>
              </div>
          </div>
      </div>
    `);

  var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  ElementsTestRunner.findNode(function() {
    return false;
  }, firstStep);

  function firstStep() {
    TestRunner.addResult('===== Initial state of tree outline =====\n');
    dump();

    var topNode = treeOutline.rootElement().childAt(0).childAt(1).childAt(0);
    topNode.expandRecursively();
    TestRunner.deprecatedRunAfterPendingDispatches(secondStep);
  }

  function secondStep() {
    TestRunner.addResult('\n===== State of tree outline after calling .expandRecursively() =====\n');
    dump();

    TestRunner.completeTest();
  }

  function dump() {
    var node = ElementsTestRunner.expandedNodeWithId('depth-1');
    ElementsTestRunner.dumpElementsTree(node);
  }
})();
