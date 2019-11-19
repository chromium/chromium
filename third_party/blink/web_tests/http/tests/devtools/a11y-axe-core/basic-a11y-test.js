// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  await TestRunner.loadModule('axe_core_test_runner');
  const locationsToTest =
    [
      // elements
      'elements.domProperties',
      // Performance Monitor
      'performance.monitor',
      // Sensors
      'sensors',
    ];

  //TODO(crbug.com/1004940): exclude scrollable-region-focusable for performance.monitor only
  const NO_SCROLLABLE_REGION_FOCUSABLE_RULESET = {
    'scrollable-region-focusable': { enabled: false, },
  };

  for (const location of locationsToTest)
    await loadViewAndTestElementViolations(location);

  TestRunner.completeTest();

  async function loadViewAndTestElementViolations(view) {
    TestRunner.addResult(`Tests accessibility in the ${view} view using the axe-core linter.`);
    await UI.viewManager.showView(view);
    const widget = await UI.viewManager.view(view).widget();
    const ruleset = view === 'performance.monitor' ? NO_SCROLLABLE_REGION_FOCUSABLE_RULESET : {};
    await AxeCoreTestRunner.runValidation(widget.element, ruleset);
  }
})();
