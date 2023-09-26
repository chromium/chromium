// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test verifies the style info overlaid on an inspected node.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      div {
          color: red;
          background-color: blue;
          font-size: 20px;
      }
      section#section-with-wide-gamut-colors {
        color: color(xyz-d50 2 0.9 0.79);
        background-color: lab(86 26 -3.8);
      }
      </style>
      <div id="empty-div"></div>
      <div id="div-with-text">I have text</div>
      <section id="section-with-wide-gamut-colors">I'm a text</section>
    `);

  await ElementsTestRunner.dumpInspectorHighlightStyleJSON('empty-div');
  await ElementsTestRunner.dumpInspectorHighlightStyleJSON('div-with-text');
  await ElementsTestRunner.dumpInspectorHighlightStyleJSON('section-with-wide-gamut-colors');
  TestRunner.completeTest();
})();
