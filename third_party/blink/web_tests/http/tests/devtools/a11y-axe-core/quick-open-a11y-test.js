// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test accessibility in Quick Open dialog\n`);
  await self.runtime.loadModulePromise('quick_open');
  await TestRunner.loadModule('axe_core_test_runner');

  QuickOpen.QuickOpen.show('');

  const dialogWidget = UI.Dialog._instance._widget;
  const filteredListWidget = dialogWidget._defaultFocusedChild;
  TestRunner.assertTrue(filteredListWidget instanceof QuickOpen.FilteredListWidget);

  await AxeCoreTestRunner.runValidation(filteredListWidget.contentElement);
  TestRunner.completeTest();
})();
