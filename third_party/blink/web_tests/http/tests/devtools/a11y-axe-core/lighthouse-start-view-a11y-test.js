// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests accessibility in the lighthouse start view using the axe-core linter.\n');
  await TestRunner.loadTestModule('axe_core_test_runner');
  await TestRunner.showPanel('lighthouse');

  const widget = await UI.viewManager.view('lighthouse').widget();
  await AxeCoreTestRunner.runValidation(widget.contentElement);

  TestRunner.completeTest();
})();
