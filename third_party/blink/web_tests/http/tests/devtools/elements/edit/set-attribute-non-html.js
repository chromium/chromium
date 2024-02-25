// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that elements panel updates dom tree structure upon setting attribute on non HTML elements. PASSes if there is no crash.\n`);
  await TestRunner.showPanel('elements');

  await TestRunner.navigatePromise('resources/set-attribute-non-html.svg');
  var targetNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        targetNode = node;
        next();
      }
      ElementsTestRunner.selectNodeWithId('node', callback);
    },

    function testSetAttributeText(next) {
      function callback(error) {
        next();
      }
      targetNode.setAttribute('foo', 'foo2=\'baz2\' foo3=\'baz3\'', callback);
    }
  ]);
})();
