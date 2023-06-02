// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verify that CSS sourcemap enabling and disabling adds/removes sourcemap sources.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  Common.moduleSetting('cssSourceMapsEnabled').set(true);
  await TestRunner.addStylesheetTag('resources/sourcemap-style-1.css');
  await TestRunner.addStylesheetTag('resources/sourcemap-style-2.css');

  await Promise.all([
    TestRunner.waitForUISourceCode('sourcemap-style-1.scss'), TestRunner.waitForUISourceCode('sourcemap-style-2.scss')
  ]);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('disableCSSSourceMaps');
  Common.moduleSetting('cssSourceMapsEnabled').set(false);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('enableCSSSourceMaps');
  Common.moduleSetting('cssSourceMapsEnabled').set(true);
  await Promise.all([
    TestRunner.waitForUISourceCode('sourcemap-style-1.scss'), TestRunner.waitForUISourceCode('sourcemap-style-2.scss')
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
