// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that origin group names in the Security panel are distinct.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var originGroupNameSize = Object.keys(Security.SecurityPanelSidebarTree.OriginGroup).length;

  var deduplicatedNames = new Set();
  for (var key in Security.SecurityPanelSidebarTree.OriginGroup) {
    var name = Security.SecurityPanelSidebarTree.OriginGroup[key];
    deduplicatedNames.add(name);
  }

  TestRunner.addResult(
      'Number of names (' + originGroupNameSize + ') == number of unique names (' + deduplicatedNames.size +
      '): ' + (originGroupNameSize == deduplicatedNames.size) + ' (expected: true)');

  TestRunner.completeTest();
})();
