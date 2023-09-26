// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
    TestRunner.addResult(`This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid areas.\n`);
    await TestRunner.showPanel('elements');
    await TestRunner.loadHTML(`
        <style>
        body {
            width: 1000px;
            height: 1000px;
            background-color: grey;
        }
        #grid-with-areas {
            position: absolute;
            top: 0;
            left: 0;
            width: 401px;
            height: 601px;
            display: grid;
            grid-gap: 10px;
            grid-template-columns: 1fr 1fr;
            grid-template-rows: 1fr 1fr;
            grid-template-areas:
            "header  header"
            "sidebar main";
        }
        #grid-with-areas .header { grid-area: header; }
        #grid-with-areas .sidebar { grid-area: sidebar; }
        #grid-with-areas .main { grid-area: main; }
        </style>
        <div id="grid-with-areas">
            <div class="header">header</div>
            <div class="sidebar">sidebar</div>
            <div class="main">main</div>
        </div>

        <p id="description">This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid areas.</p>
      `);

    function dumpGridHighlight(id) {
      return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
    }

    await dumpGridHighlight('grid-with-areas');

    TestRunner.completeTest();
  })();
