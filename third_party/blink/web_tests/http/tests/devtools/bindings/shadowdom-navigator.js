// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that navigator shows proper UISourceCodes as shadow dom styles and scripts are added and removed.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
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

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachShadow1');
  await Promise.all([
    BindingsTestRunner.attachShadowDOM('shadow1', '#template'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachShadow2');
  await Promise.all([
    BindingsTestRunner.attachShadowDOM('shadow2', '#template'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachShadow1');
  await Promise.all([
    BindingsTestRunner.detachShadowDOM('shadow1'),
    BindingsTestRunner.waitForStyleSheetRemoved('sourcemap-style.css'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachShadow2');
  await Promise.all([
    BindingsTestRunner.detachShadowDOM('shadow2'),
    BindingsTestRunner.waitForStyleSheetRemoved('sourcemap-style.css'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
