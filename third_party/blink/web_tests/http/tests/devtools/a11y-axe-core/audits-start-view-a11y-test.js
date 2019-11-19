// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
    TestRunner.addResult('Tests accessibility in the audits start view using the axe-core linter.\n');
    await TestRunner.loadModule('axe_core_test_runner');
    await TestRunner.loadModule('audits_test_runner');
    await TestRunner.showPanel('audits');
    await AxeCoreTestRunner.runValidation(AuditsTestRunner.getContainerElement());
    TestRunner.completeTest();
})();
