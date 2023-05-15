// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(
      `Verify that UISourceCodes are added and removed as shadow dom styles and scripts are added and removed.\n`);
  await TestRunner.loadHTML(`
    <template id='template'>
    <style>div {
        color: blue;
    }
    /*# sourceURL=sourcemap-style.css */
    /*# sourceMappingURL=${TestRunner.url('resources/sourcemap-style.css.map')} */
    </style>
    <script>window.foo = console.log.bind(console, 'foo');
    //# sourceURL=sourcemap-script.js
    //# sourceMappingURL=${TestRunner.url('resources/sourcemap-script.js.map')}
    </script>
    <p>Hi! I'm ShadowDOM v1!</p>
    </template>
  `);

  TestRunner.markStep('dumpInitialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('attachShadow1');
  await Promise.all([
    BindingsTestRunner.attachShadowDOM('shadow1', '#template', '_test_attachShadow1.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('attachShadow2');
  await Promise.all([
    BindingsTestRunner.attachShadowDOM('shadow2', '#template', '_test_attachShadow2.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachShadow1');
  await Promise.all([
    BindingsTestRunner.detachShadowDOM('shadow1', '_test_detachShadow1.js'),
    BindingsTestRunner.waitForStyleSheetRemoved('sourcemap-style.css'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachShadow2');
  await Promise.all([
    BindingsTestRunner.detachShadowDOM('shadow2', '_test_detachShadow2.js'),
    BindingsTestRunner.waitForStyleSheetRemoved('sourcemap-style.css'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
