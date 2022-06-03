// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests stability of performance metrics list.\n`);

  const model = SDK.targetManager.mainTarget().model(SDK.PerformanceMetricsModel);
  await model.enable();
  let metrics = (await model.requestMetrics()).metrics;

  TestRunner.addResult('\nMetrics reported:');
  TestRunner.addResults([...metrics.keys()].sort());

  TestRunner.completeTest();
})();
