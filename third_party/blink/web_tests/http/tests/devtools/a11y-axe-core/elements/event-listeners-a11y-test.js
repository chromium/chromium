// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
    // axe-core issue #1444 -- role="tree" requires children with role="treeitem",
    // but it is reasonable to have trees with no leaves.
    const NO_REQUIRED_CHILDREN_RULESET = {
      'aria-required-children': {
        enabled: false,
      },
    };
    const DEFAULT_RULESET = { };

    TestRunner.addResult(
        'Tests accessibility in DOM eventlistener pane using axe-core linter.');

    const view = 'elements.event-listeners';
    const widget = await UI.ViewManager.ViewManager.instance().view(view).widget();
    await UI.ViewManager.ViewManager.instance().showView(view);

    const treeElement = widget.element;
    TestRunner.addResult('Running the axe-core linter on tree element.');
    await AxeCoreTestRunner.runValidation(treeElement, NO_REQUIRED_CHILDREN_RULESET);

    const toolbarElement = treeElement.parentElement.querySelector('.toolbar');
    TestRunner.addResult('Running the axe-core linter on toolbar.');
    await AxeCoreTestRunner.runValidation(toolbarElement, DEFAULT_RULESET);

    TestRunner.completeTest();
  })();
