// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that editing sourcecode which is referred by multiple stylesheets (via sourceURL comment) updates all stylesheets.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="inspected">Inspected node</div>

      <style>div{color:red;}
      /*# sourceURL=stylesheet.css */
      </style>

      <template id="template">
      <style>div{color:red;}
      /*# sourceURL=stylesheet.css */
      </style>
      <p>Hi! I'm ShadowDOM v1!</p>
      </template>
    `);

  await BindingsTestRunner.attachShadowDOM('shadow1', '#template'),
      await BindingsTestRunner.attachFrame('frame', './resources/frame.html');
  var uiSourceCode = await TestRunner.waitForUISourceCode('stylesheet.css');
  var headers =
      TestRunner.cssModel.styleSheetHeaders().filter(header => header.resourceURL().endsWith('stylesheet.css'));
  TestRunner.addResult('Headers count: ' + headers.length);

  TestRunner.markStep('Make edits with Sources Panel');
  var sourceFrame = await new Promise(x => SourcesTestRunner.showScriptSource('stylesheet.css', x));
  SourcesTestRunner.replaceInSource(sourceFrame, 'red', 'EDITED');
  await TestRunner.addSnifferPromise(Bindings.StyleFile.prototype, '_styleFileSyncedForTest');
  await checkHeadersContent();


  TestRunner.markStep('Make edits via css model');
  TestRunner.cssModel.setStyleSheetText(headers[0].id, '* { --foo: "bar" }');
  await TestRunner.addSnifferPromise(Bindings.StyleFile.prototype, '_styleFileSyncedForTest');
  await checkHeadersContent();
  TestRunner.completeTest();


  async function checkHeadersContent(expected) {
    var contents = await Promise.all(headers.map(header => header.requestContent()));
    contents = contents.map(c => c.content);
    contents.push(uiSourceCode.workingCopy());
    var dedup = new Set(contents);
    if (dedup.size !== 1) {
      TestRunner.addResult('ERROR: contents are out of sync!');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult('Both headers and uiSourceCode content:');
    TestRunner.addResult(dedup.firstValue());
  }
})();
