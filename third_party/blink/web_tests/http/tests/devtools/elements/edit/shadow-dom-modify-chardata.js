// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that elements panel updates shadow dom tree structure upon typing.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container"><input type="text" id="input1"></div>
      <script>
      function typeText()
      {
          var input = document.getElementById("input1");
          input.focus();
          eventSender.keyDown("B");
          eventSender.keyDown("a");
          eventSender.keyDown("r");
      }
      </script>
  `);

  var containerNode;
  Common.settingForTest('showUAShadowDOM').set(true);
  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        containerNode = ElementsTestRunner.expandedNodeWithId('container');
        TestRunner.addResult('========= Original ========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
      ElementsTestRunner.expandElementsTree(callback);
    },

    function testAppend(next) {
      TestRunner.evaluateInPage('typeText()', callback);

      function callback() {
        TestRunner.addResult('======== Type text =========');
        ElementsTestRunner.dumpElementsTree(containerNode);
        next();
      }
    }
  ]);
})();
