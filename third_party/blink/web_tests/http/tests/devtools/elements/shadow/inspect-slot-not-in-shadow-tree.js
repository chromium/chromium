// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that slots that are not in a shadow tree can be inspected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <slot id="test1"><span>test</span></slot>
    `);

  TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(finishTest);
  ConsoleTestRunner.evaluateInConsole('inspect(test1)');
  function finishTest() {
    TestRunner.addResult('Inspect successful');
    TestRunner.completeTest();
  }
})();
