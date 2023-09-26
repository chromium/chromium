// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the base size returned when highlighting flex items.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .container {
        display: flex;
        position: absolute;
        width: 150px;
        height: 150px;
        outline: 1px dashed red;
      }
      </style>
      <div class="container" style="top:50px;left:50px;">
        <div id="fixed-flex-basis" style="flex:1 1 20px;">Defined flex-basis in px</div>
      </div>
      <div class="container" style="top:50px;left:250px;">
        <div id="zero-flex-basis" style="flex:1;">flex-basis 0</div>
      </div>
      <div class="container" style="top:250px;left:50px;">
        <div id="fixed-width" style="flex-grow:1;width:30px;">Defined width in px</div>
      </div>
      <div class="container" style="top:250px;left:250px;">
        <div id="missing-basis-width" style="flex-grow:1;">No flex-basis or width</div>
      </div>
      <div class="container" style="flex-direction:column;top:450px;left:50px;">
        <div id="column-fixed-height" style="flex-grow:1;height:40px;">Defined height in px</div>
      </div>
      <p id="description">This test verifies the base size returned when highlighting flex items.</p>
    `);

  function dumFlexHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id,
      ['flexItemInfo'],
      resolve));
  }

  await dumFlexHighlight('fixed-flex-basis');
  await dumFlexHighlight('zero-flex-basis');
  await dumFlexHighlight('fixed-width');
  await dumFlexHighlight('missing-basis-width');
  await dumFlexHighlight('column-fixed-height');

  TestRunner.completeTest();
})();
