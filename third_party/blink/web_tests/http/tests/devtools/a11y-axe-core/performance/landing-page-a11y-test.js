// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

(async function() {
  TestRunner.addResult('Tests accessibility in Performance landing page using the axe-core linter.');

  const view = 'timeline';
  await UI.viewManager.showView(view);
  const widget = await UI.viewManager.view(view).widget();
  await AxeCoreTestRunner.runValidation(widget.element);

  TestRunner.completeTest();
})();
