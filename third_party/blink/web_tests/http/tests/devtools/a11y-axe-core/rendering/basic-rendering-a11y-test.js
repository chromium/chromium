// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {

  TestRunner.addResult('Tests accessibility in the rendering view using the axe-core linter.');
  await UI.ViewManager.ViewManager.instance().showView('rendering');
  const renderingView = await UI.ViewManager.ViewManager.instance().view('rendering').widget();
  await AxeCoreTestRunner.runValidation(renderingView.element);

  TestRunner.completeTest();
})();
