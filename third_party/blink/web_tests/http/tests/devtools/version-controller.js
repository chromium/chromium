// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests inspector version controller.\n`);


  function createBreakpoint(uiSourceCodeId, lineNumber, condition, enabled) {
    return {sourceFileId: uiSourceCodeId, lineNumber: lineNumber, condition: condition, enabled: enabled};
  }

  TestRunner.runTestSuite([
    function testMethodsToRunToUpdateVersion(next) {
      function runVersionControllerTest(oldVersion, currentVersion) {
        TestRunner.addResult('Testing methods to run to upgrade from ' + oldVersion + ' to ' + currentVersion + '.');
        var versionController = new Common.Settings.VersionController();
        var methodsToRun = versionController.methodsToRunToUpdateVersion(oldVersion, currentVersion);
        TestRunner.addResult('Methods to run: ' + JSON.stringify(methodsToRun));
        TestRunner.addResult('');
      }

      runVersionControllerTest(0, 0);
      runVersionControllerTest(0, 1);
      runVersionControllerTest(0, 2);
      runVersionControllerTest(0, 3);
      runVersionControllerTest(1, 1);
      runVersionControllerTest(1, 2);
      runVersionControllerTest(1, 3);
      runVersionControllerTest(2, 2);
      runVersionControllerTest(2, 3);
      next();
    },

    function testClearBreakpointsWhenTooMany(next) {
      function runClearBreakpointsTest(breakpointsCount, maxBreakpointsCount) {
        TestRunner.addResult(
            'Starting test with ' + breakpointsCount + ' breakpoints and ' + maxBreakpointsCount + ' allowed at max.');
        var versionController = new Common.Settings.VersionController();
        var serializedBreakpoints = [];
        for (var i = 0; i < breakpointsCount; ++i)
          serializedBreakpoints.push(createBreakpoint('file' + i + '.js', i % 10, '', true));
        var breakpointsSetting = new TestRunner.MockSetting(serializedBreakpoints);
        versionController.clearBreakpointsWhenTooMany(breakpointsSetting, maxBreakpointsCount);
        TestRunner.addResult(
            'Number of breakpoints left in the setting after the test: ' + breakpointsSetting.get().length + '.');
        TestRunner.addResult('');
      }

      runClearBreakpointsTest(0, 500);
      runClearBreakpointsTest(1, 500);
      runClearBreakpointsTest(2, 500);
      runClearBreakpointsTest(499, 500);
      runClearBreakpointsTest(500, 500);
      runClearBreakpointsTest(501, 500);
      runClearBreakpointsTest(1000, 500);
      next();
    }
  ]);
})();
