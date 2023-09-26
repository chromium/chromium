// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that DOMNode properly tracks own and descendants' user properties.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <div id="container">
        <div id="child1"></div>
        <div id="child2"><a href="#" id="aNode">Third-level node</a></div>
      </div>
    `);

  var containerNode;
  var child1Node;
  var child2Node;
  var aNode;

  ElementsTestRunner.expandElementsTree(step0);

  function step0() {
    containerNode = ElementsTestRunner.expandedNodeWithId('container');
    child1Node = ElementsTestRunner.expandedNodeWithId('child1');
    child2Node = ElementsTestRunner.expandedNodeWithId('child2');
    aNode = ElementsTestRunner.expandedNodeWithId('aNode');

    aNode.setMarker('attr1', true);
    TestRunner.addResult('attr1 set on aNode');
    ElementsTestRunner.dumpElementsTree(null);

    child2Node.setMarker('attr2', 'value');
    TestRunner.addResult('attr2 set on child2');
    ElementsTestRunner.dumpElementsTree(null);

    child2Node.setMarker('attr1', true);
    TestRunner.addResult('attr1 set on child2');
    ElementsTestRunner.dumpElementsTree(null);

    aNode.setMarker('attr1', 'anotherValue');
    TestRunner.addResult('attr1 modified on aNode');
    ElementsTestRunner.dumpElementsTree(null);

    child2Node.setMarker('attr2', 'anotherValue');
    TestRunner.addResult('attr2 modified on child2');
    ElementsTestRunner.dumpElementsTree(null);

    aNode.setMarker('attr1', null);
    TestRunner.addResult('attr1 removed from aNode');
    ElementsTestRunner.dumpElementsTree(null);

    aNode.removeNode(step1);
  }

  function step1(error) {
    if (error) {
      TestRunner.addResult('Failed to remove aNode');
      TestRunner.completeTest();
      return;
    }

    TestRunner.addResult('aNode removed');
    ElementsTestRunner.dumpElementsTree(null);

    child2Node.removeNode(step2);
  }

  function step2(error) {
    if (error) {
      TestRunner.addResult('Failed to remove child2');
      TestRunner.completeTest();
      return;
    }

    TestRunner.addResult('child2 removed');
    ElementsTestRunner.dumpElementsTree(null);
    TestRunner.completeTest();
  }
})();
