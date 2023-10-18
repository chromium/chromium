// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that FailedToParseScriptSource event is NOT raised after running a damaged part of a script that was already parsed.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function thisIsBroken() {
        const duplicated = 1;
        const duplicated = 2;
      }
  `);

  TestRunner.debuggerModel.addEventListener(
    SDK.DebuggerModel.Events.FailedToParseScriptSource, () => {
      TestRunner.addResult('ERROR: Recieved script failed to parse event')
    });
  await TestRunner.evaluateInPagePromise('thisIsBroken()');
  TestRunner.addResult('Finished running broken code.');
  TestRunner.completeTest();
})();
