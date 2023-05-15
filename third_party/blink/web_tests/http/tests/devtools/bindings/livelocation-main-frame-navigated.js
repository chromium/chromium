// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(`Verify that debugger live location gets updated.\n`);

  TestRunner.markStep('attachFrame');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame', './resources/sourcemap-frame.html'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);

  const jsLiveLocation = await BindingsTestRunner.createDebuggerLiveLocation(
      'js', 'sourcemap-script.js', undefined, undefined,
      /* dumpOnUpdate= */ false);
  await BindingsTestRunner.dumpLocation(jsLiveLocation, '[ CREATE ]');
  const cssLiveLocation = await BindingsTestRunner.createCSSLiveLocation(
      'css', 'sourcemap-style.css', undefined, undefined,
      /* dumpOnUpdate= */ false);
  await BindingsTestRunner.dumpLocation(cssLiveLocation, '[ CREATE ]');

  TestRunner.markStep('navigateMainFrame');
  const url = TestRunner.url('resources/empty-page.html');
  await TestRunner.navigatePromise(url);
  await BindingsTestRunner.dumpLocation(jsLiveLocation);
  await BindingsTestRunner.dumpLocation(cssLiveLocation);

  TestRunner.completeTest();
})();
