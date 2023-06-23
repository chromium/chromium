// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {

  TestRunner.addResult(
      'Tests accessibility in the settings workspace view using the axe-core linter.');

  const fs = new BindingsTestRunner.TestFileSystem('file:///this/is/a/test');
  await fs.reportCreatedPromise();

  await UI.viewManager.showView('workspace');
  const workspaceWidget = await UI.viewManager.view('workspace').widget();

  await AxeCoreTestRunner.runValidation(workspaceWidget.element);
  TestRunner.completeTest();
})();
