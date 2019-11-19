// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that sourcemap sources are updated when a new sourcemap is added`);
  var contents = [
    `console.log(1);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZXZhbC1pbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbImV2YWwtaW4iXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsT0FBTyxDQUFDLEdBQUcsQ0FBQyxDQUFDLENBQUMsQ0FBQyIsInNvdXJjZXNDb250ZW50IjpbImNvbnNvbGUubG9nKDEpOyJdfQ==`,
    // same mappings, other encoded sourcesContent
    `console.log(2);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZXZhbC1pbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbImV2YWwtaW4iXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsT0FBTyxDQUFDLEdBQUcsQ0FBQyxDQUFDLENBQUMsQ0FBQyIsInNvdXJjZXNDb250ZW50IjpbImNvbnNvbGUubG9nKDIpOyJdfQ==`,
    // other mappings, other sourcesContent
    `console.log(11);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZXZhbC1pbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbImV2YWwtaW4iXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsT0FBTyxDQUFDLEdBQUcsQ0FBQyxFQUFFLENBQUMsQ0FBQyIsInNvdXJjZXNDb250ZW50IjpbImNvbnNvbGUubG9nKDExKTsiXX0=`
  ];

  for (var content of contents) {
    TestRunner.addResult('Evaluating script with new source map..');
    await TestRunner.evaluateInPageAnonymously(content);
    await new Promise(resolve => TestRunner.addSniffer(Bindings.CompilerScriptMapping.prototype, "_sourceMapAttachedForTest", resolve));
    var uiSourceCode = await TestRunner.waitForUISourceCode("eval-in");
    TestRunner.addResult((await uiSourceCode.requestContent()).content);
  }
  TestRunner.completeTest();
})();

