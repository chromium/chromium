// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(
      `Tests that content is correctly shown for css loaded with invalid mime type in quirks mode. https://bugs.webkit.org/show_bug.cgi?id=80528\n`);
  await TestRunner.showPanel('resources');
  await TestRunner.addStylesheetTag('resources/stylesheet-text-plain.php');
  await TestRunner.waitForUISourceCode('stylesheet-text-plain.php');
  var cssResource;
  TestRunner.resourceTreeModel.forAllResources(resource => {
    if (resource.url.endsWith('stylesheet-text-plain.php'))
      cssResource = resource;
  });
  TestRunner.addResult(cssResource.url);
  TestRunner.assertEquals(
      cssResource.resourceType(), Common.ResourceType.resourceTypes.Stylesheet, 'Resource type should be Stylesheet.');
  TestRunner.assertTrue(!cssResource.failed, 'Resource loading failed.');
  const {isEncoded} = await cssResource.requestContent();

  var content = isEncoded ? window.atob(cssResource.content) : cssResource.content;
  TestRunner.addResult('Resource.content: ' + content);
  TestRunner.completeTest();
})();
