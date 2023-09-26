// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that editing media text updates element styles.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @media screen and (max-device-width: 100000px) {
          #inspected {
              color: green;
          }
          #inspected {
              color: blue;
          }
      }
      @media screen and (max-device-width: 200000px) {
          #other {
              color: green;
          }
      }
      </style>
      <div id="inspected" style="color: red">Text</div>
      <div id="other"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    TestRunner.addResult('=== Before media text modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var mediaTextElement = ElementsTestRunner.firstMediaTextElementInSection(section);
    mediaTextElement.click();
    mediaTextElement.textContent = 'screen and (max-device-width: 99999px)';
    ElementsTestRunner.waitForMediaTextCommitted(step2);
    mediaTextElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  async function step2() {
    TestRunner.addResult('=== After valid media text modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = ElementsTestRunner.firstMatchedStyleSection();
    var mediaTextElement = ElementsTestRunner.firstMediaTextElementInSection(section);
    mediaTextElement.click();
    mediaTextElement.textContent = 'something is wrong here';
    ElementsTestRunner.waitForMediaTextCommitted(step3);
    mediaTextElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
  }

  async function step3() {
    TestRunner.addResult('=== After invalid media text modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
