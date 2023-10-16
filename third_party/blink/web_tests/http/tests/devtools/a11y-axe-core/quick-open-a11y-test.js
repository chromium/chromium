// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as QuickOpen from 'devtools/ui/legacy/components/quick_open/quick_open.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult('Test accessibility in Quick Open dialog\n');

  QuickOpen.QuickOpen.QuickOpenImpl.show('');

  const dialogWidget = UIModule.Dialog.Dialog.instance.widget();
  const filteredListWidget = dialogWidget.defaultFocusedChild;
  TestRunner.assertTrue(filteredListWidget instanceof QuickOpen.FilteredListWidget.FilteredListWidget);

  await AxeCoreTestRunner.runValidation(filteredListWidget.contentElement);
  TestRunner.completeTest();
})();
