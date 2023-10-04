// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(
      `Verify that navigator is rendered properly when frame with dynamic script and style is added and removed.\n`);

  var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigator.show(UI.InspectorView.InspectorView.instance().element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame');
  await BindingsTestRunner.attachFrame('frame', './resources/dynamic-frame.html');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
