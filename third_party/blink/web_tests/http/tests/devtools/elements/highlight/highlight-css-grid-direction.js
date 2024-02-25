// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies that grids with direction rtl and ltr are correctly highlighted.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .grid {
        position: absolute;
	top: 44px;
	left: 8px;
        grid-template-columns: 20px 50px;
        grid-template-rows: 100px;
        width: 100px;
        display: grid;
      }
      .ltr-dir {
        direction: ltr;
      }
      .rtl-dir {
        direction: rtl;
      }
      .with-gap {
        grid-gap: 1em;
      }
      </style>

      <p id="description">This test verifies that grids with direction rtl and ltr are correctly highlighted.</p>
      <div>
          <div class="grid ltr-dir" id="ltrGrid">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid rtl-dir" id="rtlGrid">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid ltr-dir with-gap" id="ltrGridGap">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid rtl-dir with-gap" id="rtlGridGap">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
      </div>
`);
  function dumpGridHighlight(id) {
    return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  }

  await dumpGridHighlight('ltrGrid');
  await dumpGridHighlight('rtlGrid');
  await dumpGridHighlight('ltrGridGap');
  await dumpGridHighlight('rtlGridGap');

  TestRunner.completeTest();
})();
