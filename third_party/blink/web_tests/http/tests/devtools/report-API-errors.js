// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';

(async function() {
  TestRunner.addResult(`Tests that InspectorBackendStub is catching incorrect arguments.\n`);


  console.error = function() {
    TestRunner.addResult(Platform.StringUtilities.sprintf.apply(this, arguments));
  };

  TestRunner.NetworkAgent.invoke_setUserAgentOverride({userAgent: 1});
  TestRunner.NetworkAgent.invoke_setUserAgentOverride({});
  TestRunner.NetworkAgent.invoke_setUserAgentOverride({userAgent: '123', acceptLanguage: 'not a function'});
  TestRunner.NetworkAgent.invoke_setUserAgentOverride({userAgent: '123', acceptLanguage: undefined});
  TestRunner.RuntimeAgent.invoke_evaluate({expression: 'true', objectGroup: 'test'});
  TestRunner.RuntimeAgent.invoke_evaluate({expression: 'true', objectGroup: 'test', includeCommandLineAPI: function() {}});
  TestRunner.RuntimeAgent.invoke_evaluate({expression: 'true', objectGroup: 'test', includeCommandLineAPI: undefined, silent: function() {}});
  TestRunner.mainTarget._router._onMessage('{"method": "wrongDomain.something-strange", "params": {}}');
  TestRunner.mainTarget._router._onMessage('{"method": "Inspector.something-strange", "params": {}}');

  TestRunner.completeTest();
})();
