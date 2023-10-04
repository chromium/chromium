// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

const GC = async () => {
  await TestRunner.evaluateInPageAsync(`new Promise(resolve =>
    GCController.asyncCollectAll(resolve))`);
};

(async function () {
  TestRunner.addResult(`Verify that SourceMap sources are correctly displayed in navigator.\n`);

  var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigator.show(UI.InspectorView.InspectorView.instance().element);

  TestRunner.markStep('initialWorkspace');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame1');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/sourcemap-frame.html', '_test_create-iframe1.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame2');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame2', './resources/sourcemap-frame.html', '_test_create-iframe2.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
