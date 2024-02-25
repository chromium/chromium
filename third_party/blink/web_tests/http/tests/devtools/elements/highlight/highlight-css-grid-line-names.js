// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the names and positions of named grid lines are generated correctly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style>
    body {
        width: 1000px;
        height: 1000px;
    }
    #grid-with-line-names {
        position: absolute;
        top: 0;
        left: 0;
        display: grid;
        gap: 10px;
        grid-template-columns: [fullpage-start] 100px [content-start header] 500px [content-end] 20px [fullpage-end];
        grid-template-rows: [header] 200px [main article images] repeat(3, [section] 200px) [end];
    }
    </style>
    <div id="grid-with-line-names"></div>

    <p id="description">This test verifies the names and positions of named grid lines are generated correctly.</p>
  `);

  ElementsTestRunner.dumpInspectorHighlightJSON('grid-with-line-names', () => TestRunner.completeTest());
})();
