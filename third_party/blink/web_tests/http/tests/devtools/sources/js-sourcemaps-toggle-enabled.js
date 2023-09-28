// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Verify that JavaScript sourcemap enabling and disabling adds/removes sourcemap sources.\n`);
  await TestRunner.showPanel('sources');

  var sourcesNavigator = new SourcesModule.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  Common.Settings.moduleSetting('jsSourceMapsEnabled').set(true);
  TestRunner.addScriptTag('resources/sourcemap-script.js');
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts');

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('disableJSSourceMaps');
  Common.Settings.moduleSetting('jsSourceMapsEnabled').set(false);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('enableJSSourceMaps');
  Common.Settings.moduleSetting('jsSourceMapsEnabled').set(true);
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts'),
      SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
