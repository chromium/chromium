// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult('Checks that we update breakpoint location on source map loading error.');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  const brokenSourceMap = {"version":3,"file":"a.js","sourceRoot":"","sources":["a.ts"],"names":[],"mappings":"AAAA;IACE,OAAO,CAAC,GAAG,CAAC,EAAE,CAAC,CAAC;AAClB,CAAC","sourcesContent":["function foo() {\n  console.log(42);\n}\n"]};
  const sourceMapURL = 'data:application/json;base64,' + btoa(JSON.stringify(brokenSourceMap)+'\n');

  TestRunner.evaluateInPageAnonymously(`function foo() {
  console.log(42);
}
//# sourceMappingURL=${sourceMapURL}
//# sourceURL=foo.js`);

  let sourceFrame = await new Promise(resolve => SourcesTestRunner.showScriptSource('a.ts', resolve));
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 1, false);
  await SourcesTestRunner.waitBreakpointSidebarPane(true);
  SourcesTestRunner.dumpBreakpointSidebarPane();

  await new Promise(resolve => TestRunner.reloadPage(resolve));

  let origLoad = Host.ResourceLoader.load;
  let stopRequest;
  let sourceMapRequested;
  let sourceMapRequest = new Promise(resolve => sourceMapRequested = resolve);
  Host.ResourceLoader.setLoadForTest(function(url, headers, callback){
    if (url === sourceMapURL) {
      stopRequest = () => callback(false, [], "", {message:"<error message>"});
      sourceMapRequested();
      return;
    }
    return origLoad.apply(this, arguments);
  });

  await TestRunner.evaluateInPageAnonymously(`function foo() {
  console.log(42);
}
//# sourceMappingURL=${sourceMapURL}
//# sourceURL=foo.js`);

  await Promise.all([SourcesTestRunner.waitBreakpointSidebarPane(true), sourceMapRequest]);
  SourcesTestRunner.dumpBreakpointSidebarPane();
  stopRequest();
  await SourcesTestRunner.waitBreakpointSidebarPane(true);
  SourcesTestRunner.dumpBreakpointSidebarPane();
  TestRunner.completeTest();
})();
