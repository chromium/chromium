// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`The test verifies that extension names are resolved properly in navigator view.\n`);
  await TestRunner.showPanel('sources');

  var contentScriptsNavigatorView = new Sources.SourcesNavigator.ContentScriptsNavigatorView();
  contentScriptsNavigatorView.show(UI.InspectorView.InspectorView.instance().element);

  var mockExecutionContext =
      {id: 1234567, origin: 'chrome-extension://113581321345589144', name: 'FibExtension', auxData: {isDefault: false}};
  var mockContentScriptURL = mockExecutionContext.origin + '/script.js';

  TestRunner.runTestSuite([
    async function testAddExecutionContextBeforeFile(next) {
      TestRunner.runtimeModel.executionContextCreated(mockExecutionContext);
      await SourcesTestRunner.addScriptUISourceCode(mockContentScriptURL, '', true, 1234567);
      SourcesTestRunner.dumpNavigatorView(contentScriptsNavigatorView);
      next();
    },
  ]);
})();
