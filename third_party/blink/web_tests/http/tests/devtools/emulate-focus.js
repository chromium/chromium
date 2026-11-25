// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Main from 'devtools/entrypoints/main/main.js';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that focus emulation works.\n`);
  await dumpPageFocus();

  TestRunner.addResult('\nEmulating page focus');
  Main.MainImpl.MainImpl.universeForTest.settings
      .resolve(SDK.SDKSettings.emulatePageFocusSettingDescriptor)
      .set(true);
  await dumpPageFocus();

  TestRunner.addResult('\nDisabling focus emulation');
  Main.MainImpl.MainImpl.universeForTest.settings
      .resolve(SDK.SDKSettings.emulatePageFocusSettingDescriptor)
      .set(false);
  await dumpPageFocus();


  async function dumpPageFocus() {
    const pageHasFocus = await TestRunner.evaluateInPagePromise('document.hasFocus()');
    TestRunner.addResult(`document.hasFocus(): ${pageHasFocus}`);
  }

  TestRunner.completeTest();
})();
