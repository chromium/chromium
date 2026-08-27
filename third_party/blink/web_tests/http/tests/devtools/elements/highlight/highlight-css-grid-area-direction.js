// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {dumpStableInspectorHighlightJSON} from './resources/highlight-test-helper.js';

(async function() {
  TestRunner.addResult(`This test verifies that grid areas with direction rtl and ltr are correctly highlighted.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      body {
        width: 1000px;
        height: 1000px;
        background-color: grey;
      }
      .grid {
        position: absolute;
        top: 0;
        left: 0;
        width: 400px;
        height: 300px;
        display: grid;
        grid-template-columns: 100px 200px;
        grid-template-rows: 150px 150px;
        grid-template-areas:
          "a b"
          "c d";
      }
      .with-gap {
        grid-gap: 20px 10px;
        grid-template-columns: 100px 100px 100px;
        grid-template-rows: 100px 100px;
        grid-template-areas:
          "header  header  header"
          "sidebar main    main";
      }
      .ltr-dir {
        direction: ltr;
      }
      .rtl-dir {
        direction: rtl;
      }
      </style>

      <div class="grid ltr-dir" id="ltrGrid">
        <div style="grid-area: a">a</div>
        <div style="grid-area: b">b</div>
        <div style="grid-area: c">c</div>
        <div style="grid-area: d">d</div>
      </div>
      <div class="grid rtl-dir" id="rtlGrid">
        <div style="grid-area: a">a</div>
        <div style="grid-area: b">b</div>
        <div style="grid-area: c">c</div>
        <div style="grid-area: d">d</div>
      </div>
      <div class="grid ltr-dir with-gap" id="ltrGridGap">
        <div style="grid-area: header">header</div>
        <div style="grid-area: sidebar">sidebar</div>
        <div style="grid-area: main">main</div>
      </div>
      <div class="grid rtl-dir with-gap" id="rtlGridGap">
        <div style="grid-area: header">header</div>
        <div style="grid-area: sidebar">sidebar</div>
        <div style="grid-area: main">main</div>
      </div>

      <p id="description">This test verifies that grid areas with direction rtl and ltr are correctly highlighted.</p>
    `);

  await dumpStableInspectorHighlightJSON('ltrGrid');
  await dumpStableInspectorHighlightJSON('rtlGrid');
  await dumpStableInspectorHighlightJSON('ltrGridGap');
  await dumpStableInspectorHighlightJSON('rtlGridGap');

  TestRunner.completeTest();
})();
