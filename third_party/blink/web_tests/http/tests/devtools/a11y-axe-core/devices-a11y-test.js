// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  TestRunner.addResult(
      'Tests accessibility in the remote-devices landing page using the axe-core linter.');

  await UI.viewManager.showView('remote-devices');
  const widget = await UI.viewManager.view('remote-devices').widget();

  widget._discoveryView._portForwardingView._addRuleButtonClicked();

  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
