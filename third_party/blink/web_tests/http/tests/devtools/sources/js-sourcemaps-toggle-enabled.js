// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verify that JavaScript sourcemap enabling and disabling adds/removes sourcemap sources.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  Common.moduleSetting('jsSourceMapsEnabled').set(true);
  TestRunner.addScriptTag('resources/sourcemap-script.js');
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts');

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('disableJSSourceMaps');
  Common.moduleSetting('jsSourceMapsEnabled').set(false);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('enableJSSourceMaps');
  Common.moduleSetting('jsSourceMapsEnabled').set(true);
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts'),
      SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
