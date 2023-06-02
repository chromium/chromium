// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`This text should be autosized to 40px computed font-size (16 * 800/320).\n`);
  await TestRunner.loadHTML(`
      <div id="measure">
        <br>
        FAIL IF THIS IS VISIBLE<br>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      if (window.internals)
          internals.settings.setTextAutosizingWindowSizeOverride(320, 480);

      function getAutosizing()
      {
          return JSON.stringify({ textHeight: document.getElementById('measure').offsetHeight });
      }
  `);

  function assertAutosizingResult(expected, callback) {
    function resultCallback(jsonResult) {
      var result = JSON.parse(jsonResult);
      var actual = result.textHeight > 200;
      TestRunner.addResult(
          'Text ' + (actual ? 'was' : 'was not') + ' autosized. ' + (expected == actual ? 'PASS' : 'FAIL'));
      if (callback)
        callback();
    }
    TestRunner.evaluateInPage('getAutosizing()', resultCallback);
  }

  TestRunner.runTestSuite([
    function checkNotAutosizedWithoutPageAgent(next) {
      TestRunner.PageAgent.disable();
      assertAutosizingResult(false, next);
    },

    function checkNotAutosizedWithPageAgent(next) {
      TestRunner.PageAgent.enable();
      assertAutosizingResult(false, next);
    },

    function checkNotAutosizedWithZeroDeviceMetrics(next) {
      TestRunner.PageAgent.invoke_setDeviceMetricsOverride(
          {width: 0, height: 0, deviceScaleFactor: 0, mobile: false, fitWindow: true});
      assertAutosizingResult(false, next);
    },

    function checkAutosizedWhenDeviceMetrics(next) {
      TestRunner.PageAgent.invoke_setDeviceMetricsOverride(
          {width: 320, height: 480, deviceScaleFactor: 0, mobile: true, fitWindow: true});
      assertAutosizingResult(true, next);
    },

    function checkCleanupOverrides(next) {
      TestRunner.PageAgent.clearDeviceMetricsOverride();
      assertAutosizingResult(false);
      TestRunner.evaluateInPage('document.getElementById(\'measure\').remove();', next);
    }
  ]);
})();
