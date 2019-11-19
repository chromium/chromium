// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests memory trend calculation.\n');

  const linearRegression = new SDK.IsolateManager.MemoryTrend(4);
  TestRunner.addResult(`initial: count=${linearRegression.count()} slope=${linearRegression.fitSlope()}`);
  let x = [1, 2, 3, 4, 5, 6, 7, 9];
  let y = [1, 2, 4, 5, 5, 4, 3, 1];
  for (let i = 0; i < x.length; ++i) {
    linearRegression.add(y[i], x[i]);
    TestRunner.addResult(`(${x[i]}, ${y[i]}):  count=${linearRegression.count()} slope=${linearRegression.fitSlope()}`);
  }
  TestRunner.completeTest();
})();
