// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that focus emulation works.\n`);
  await dumpPageFocus();

  TestRunner.addResult('\nEmulating page focus');
  Common.settings.moduleSetting('emulatePageFocus').set(true);
  await dumpPageFocus();

  TestRunner.addResult('\nDisabling focus emulation');
  Common.settings.moduleSetting('emulatePageFocus').set(false);
  await dumpPageFocus();


  async function dumpPageFocus() {
    const pageHasFocus = await TestRunner.evaluateInPagePromise('document.hasFocus()');
    TestRunner.addResult(`document.hasFocus(): ${pageHasFocus}`);
  }

  TestRunner.completeTest();
})();
