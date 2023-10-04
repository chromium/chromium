// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function () {
  TestRunner.addResult(`Verify that SourceMap bindings are generating UISourceCodes properly.\n`);

  var contentScriptsNavigator = new Sources.SourcesNavigator.ContentScriptsNavigatorView();
  contentScriptsNavigator.show(UI.InspectorView.InspectorView.instance().element);

  TestRunner.markStep('initialWorkspace');
  SourcesTestRunner.dumpNavigatorView(contentScriptsNavigator, false);

  TestRunner.markStep('attachFrame1');
  await BindingsTestRunner.attachFrame('frame1', './resources/contentscript-frame.html', 'test_attachFrame1.js'),
    SourcesTestRunner.dumpNavigatorView(contentScriptsNavigator, false);

  TestRunner.markStep('attachFrame2');
  await BindingsTestRunner.attachFrame('frame2', './resources/contentscript-frame.html', 'test_attachFrame2.js'),
    SourcesTestRunner.dumpNavigatorView(contentScriptsNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', 'test_detachFrame1.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  SourcesTestRunner.dumpNavigatorView(contentScriptsNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', 'test_detachFrame2.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  SourcesTestRunner.dumpNavigatorView(contentScriptsNavigator, false);

  TestRunner.completeTest();
})();
