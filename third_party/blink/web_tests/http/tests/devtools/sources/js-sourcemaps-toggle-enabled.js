// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Main from 'devtools/entrypoints/main/main.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import {SourcesTestRunner} from 'sources_test_runner';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Verify that JavaScript sourcemap enabling and disabling adds/removes sourcemap sources.\n`);
  await TestRunner.showPanel('sources');

  var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView(Main.MainImpl.MainImpl.universeForTest.networkProjectManager);
  sourcesNavigator.show(UI.InspectorView.InspectorView.instance().element);

  Main.MainImpl.MainImpl.universeForTest.settings
      .resolve(SDK.SDKSettings.jsSourceMapsEnabledSettingDescriptor)
      .set(true);
  TestRunner.addScriptTag('resources/sourcemap-script.js');
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts');

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('disableJSSourceMaps');
  Main.MainImpl.MainImpl.universeForTest.settings
      .resolve(SDK.SDKSettings.jsSourceMapsEnabledSettingDescriptor)
      .set(false);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('enableJSSourceMaps');
  Main.MainImpl.MainImpl.universeForTest.settings
      .resolve(SDK.SDKSettings.jsSourceMapsEnabledSettingDescriptor)
      .set(true);
  await TestRunner.waitForUISourceCode('sourcemap-typescript.ts'),
      SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
