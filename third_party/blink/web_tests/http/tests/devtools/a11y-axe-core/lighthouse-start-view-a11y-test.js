// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Tests accessibility in the lighthouse start view using the axe-core linter.\n');
  await TestRunner.showPanel('lighthouse');

  const widget = await UI.ViewManager.ViewManager.instance().view('lighthouse').widget();
  await AxeCoreTestRunner.runValidation(widget.contentElement);

  TestRunner.completeTest();
})();
