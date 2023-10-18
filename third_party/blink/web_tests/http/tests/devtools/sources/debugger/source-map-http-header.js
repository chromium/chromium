// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that SourceMap and X-SourceMap http headers are propagated to scripts in the front-end.\n`);
  await TestRunner.showPanel('sources');
  await SourcesTestRunner.startDebuggerTestPromise();
  var debuggerModel = TestRunner.debuggerModel;
  debuggerModel.addEventListener(
      SDK.DebuggerModel.Events.ParsedScriptSource, onScriptAdded);
  function onScriptAdded(event) {
    var script = event.data;
    if (!event.data.contentURL().endsWith('.php'))
      return;
    TestRunner.addResult('Added script:');
    TestRunner.addResult('  url: ' + script.sourceURL);
    TestRunner.addResult('  sourceMapUrl: ' + script.sourceMapURL);
  }
  await TestRunner.navigatePromise('resources/source-map-http-header.html');
  SourcesTestRunner.completeDebuggerTest();
})();
