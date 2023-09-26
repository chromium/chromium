// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that media query stack is rendered for associated rules.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @media not print {
          #main { background:blue; }
      }

      @media print {
          #main { background: transparent; }
      }
      </style>
      <div id="main"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/media-queries.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);

  async function step1() {
    TestRunner.addResult('Main style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
