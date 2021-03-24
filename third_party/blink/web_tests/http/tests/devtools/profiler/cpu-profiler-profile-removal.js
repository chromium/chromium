// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that CPU profile removal from a group works. Bug 110466\n`);
  await TestRunner.loadTestModule('cpu_profiler_test_runner');
  await TestRunner.showPanel('js_profiler');

  await TestRunner.evaluateInPagePromise(`
      function pageFunction() {
          console.profile("p1");
          console.profileEnd("p1");
          console.profile("p1");
          console.profileEnd("p1");
          console.profile("p2");
          console.profileEnd("p2");
      }
  `);

  CPUProfilerTestRunner.startProfilerTest(function() {
    function viewLoaded() {
      var profiles = UI.panels.js_profiler;
      var type = Profiler.ProfileTypeRegistry.instance.cpuProfileType;
      while (type.getProfiles().length !== 0)
        type.removeProfile(type.getProfiles()[0]);
      TestRunner.addResult('Profile groups after removal:');
      for (var key in profiles._profileGroups)
        TestRunner.addResult(key + ': ' + profiles._profileGroups[key].length);
      var section = profiles._typeIdToSidebarSection[type.id];
      TestRunner.assertEquals(0, section.children.length, 'All children has been removed');
      CPUProfilerTestRunner.completeProfilerTest();
    }
    TestRunner.evaluateInPage('pageFunction()', function() {});
    CPUProfilerTestRunner.showProfileWhenAdded('p2');
    CPUProfilerTestRunner.waitUntilProfileViewIsShown('p2', setTimeout.bind(null, viewLoaded, 0));
  });
})();
