// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that emulated CSS media is reflected in the Styles pane.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #main { color: red; }

      @media print {
      #main { color: black; }
      }

      @media tty {
      #main { color: green; }
      }
      </style>
      <div id="main"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step0);

  async function step0() {
    TestRunner.addResult('Original style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    applyEmulatedMedia('print');
    ElementsTestRunner.waitForStyles('main', step1);
  }

  async function step1() {
    TestRunner.addResult('print media emulated:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    applyEmulatedMedia('tty');
    ElementsTestRunner.waitForStyles('main', step2);
  }

  async function step2() {
    TestRunner.addResult('tty media emulated:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    applyEmulatedMedia('');
    ElementsTestRunner.waitForStyles('main', step3);
  }

  async function step3() {
    TestRunner.addResult('No media emulated:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }

  function applyEmulatedMedia(media) {
    TestRunner.EmulationAgent.setEmulatedMedia(media);
    TestRunner.cssModel.mediaQueryResultChanged();
  }
})();
