// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Verify that navigator's 'Make a copy' works as expected.\n`);
  await TestRunner.showPanel('sources');

  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  BindingsTestRunner.addFiles(fs, {
    'script.js': {content: 'testme'},
  });
  fs.reportCreated(function() {});
  var uiSourceCode = await TestRunner.waitForUISourceCode('script.js');

  var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigator.show(UI.InspectorView.InspectorView.instance().element);
  TestRunner.addResult('BEFORE:\n' + 'file://' + fs.dumpAsText());
  sourcesNavigator.handleContextMenuCreate(uiSourceCode.project(), '', uiSourceCode);
  await TestRunner.waitForUISourceCode('NewFile');
  TestRunner.addResult('\nAFTER:\n' + 'file://' + fs.dumpAsText());
  TestRunner.completeTest();
})();
