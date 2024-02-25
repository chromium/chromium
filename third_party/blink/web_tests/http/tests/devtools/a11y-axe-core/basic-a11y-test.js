// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  const locationsToTest =
    [
      // elements
      'elements.dom-properties',
      // Performance Monitor
      'performance.monitor',
      // Sensors
      'sensors',
    ];

  // TODO(crbug.com/1004940): exclude scrollable-region-focusable for performance.monitor only
  const NO_SCROLLABLE_REGION_FOCUSABLE_RULESET = {
    'scrollable-region-focusable': { enabled: false, },
  };

  for (const location of locationsToTest) {
    await loadViewAndTestElementViolations(location);
  }

  TestRunner.completeTest();

  async function loadViewAndTestElementViolations(view) {
    TestRunner.addResult(`Tests accessibility in the ${view} view using the axe-core linter.`);
    await UI.ViewManager.ViewManager.instance().showView(view);
    const widget = await UI.ViewManager.ViewManager.instance().view(view).widget();
    const ruleset = view === 'performance.monitor' ? NO_SCROLLABLE_REGION_FOCUSABLE_RULESET : {};
    await AxeCoreTestRunner.runValidation(widget.element, ruleset);
  }
})();
