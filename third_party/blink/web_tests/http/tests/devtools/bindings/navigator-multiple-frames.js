// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function () {
  TestRunner.addResult(`Verify that navigator is properly rendered in case of multiple iframes.\n`);

  var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigator.show(UI.InspectorView.InspectorView.instance().element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('createIframes');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/magic-frame.html'),
    BindingsTestRunner.attachFrame('frame2', './resources/magic-frame.html'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2');
  await BindingsTestRunner.GC();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
