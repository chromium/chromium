// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests that focus emulation works.\n`);
  await dumpPageFocus();

  TestRunner.addResult('\nEmulating page focus');
  Common.Settings.moduleSetting('emulate-page-focus').set(true);
  await dumpPageFocus();

  TestRunner.addResult('\nDisabling focus emulation');
  Common.Settings.moduleSetting('emulate-page-focus').set(false);
  await dumpPageFocus();


  async function dumpPageFocus() {
    const pageHasFocus = await TestRunner.evaluateInPagePromise('document.hasFocus()');
    TestRunner.addResult(`document.hasFocus(): ${pageHasFocus}`);
  }

  TestRunner.completeTest();
})();
