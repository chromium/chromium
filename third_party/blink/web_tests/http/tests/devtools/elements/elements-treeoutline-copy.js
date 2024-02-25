// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that nodes can be copied in ElementsTreeOutline.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <span id="node-to-copy">This should be <b>copied</b>.</span>
    `);


  // make sure the tree is loaded
  ElementsTestRunner.selectNodeAndWaitForStyles('node-to-copy', nodeSelected);
  var input = document.body.createChild('input');

  function nodeSelected() {
    eventSender.keyDown('Copy');
    input.focus();
    setTimeout(next, 0);
  }
  function next() {
    eventSender.keyDown('Paste');
    TestRunner.addResult(input.value);
    TestRunner.completeTest();
  }
})();
