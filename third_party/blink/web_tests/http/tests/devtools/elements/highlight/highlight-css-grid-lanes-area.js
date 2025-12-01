// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
    TestRunner.addResult(`This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid Lanes areas.\n`);
    await TestRunner.showPanel('elements');
    await TestRunner.loadHTML(`
        <style>
        body {
            width: 1000px;
            height: 1000px;
            background-color: grey;
        }
        #grid-lanes-with-areas {
            width: 400px;
            height: 600px;
            display: grid-lanes;
            grid-template-columns: 2fr 1fr 1fr;
            grid-template-areas:
            "header sidebar main";
            gap: 10px;
        }
        #grid-lanes-with-areas .header { grid-area: header; }
        #grid-lanes-with-areas .sidebar { grid-area: sidebar; }
        #grid-lanes-with-areas .main { grid-area: main; }
        </style>
        <div id="grid-lanes-with-areas">
            <div class="header">header</div>
            <div class="sidebar">sidebar</div>
            <div class="main">main</div>
        </div>

        <p id="description">This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid Lanes areas.</p>
      `);

    function dumpGridLanesHighlight(id) {
      return new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
    }

    await dumpGridLanesHighlight('grid-lanes-with-areas');

    TestRunner.completeTest();
  })();
