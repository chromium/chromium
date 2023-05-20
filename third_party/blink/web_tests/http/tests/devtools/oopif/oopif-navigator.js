// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verify navigator rendering with OOPIFs`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  var sourcesNavigatorView = new Sources.NetworkNavigatorView();
  sourcesNavigatorView.show(UI.inspectorView.element);

  await TestRunner.navigatePromise('resources/page.html');

  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
  TestRunner.completeTest();
})();
