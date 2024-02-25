// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that inspecting keyframes injected via CSSOM doesn't crash.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      </style>
      <div id="element" style="animation: injected 1s infinite"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function injectAnimation()
      {
          var styleSheet = document.styleSheets[0];
          styleSheet.insertRule("@keyframes injected { 0% {opacity:0} 100% {opacity:1} }", 0);
      }
  `);

  TestRunner.evaluateInPage('injectAnimation()');
  ElementsTestRunner.selectNodeAndWaitForStyles('element', TestRunner.completeTest.bind(TestRunner));
})();
