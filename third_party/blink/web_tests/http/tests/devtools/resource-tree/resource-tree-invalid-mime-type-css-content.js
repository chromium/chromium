// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that content is correctly shown for css loaded with invalid mime type in quirks mode. https://bugs.webkit.org/show_bug.cgi?id=80528\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
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
      cssResource.resourceType(), Common.resourceTypes.Stylesheet, 'Resource type should be Stylesheet.');
  TestRunner.assertTrue(!cssResource.failed, 'Resource loading failed.');
  await cssResource.requestContent();

  var content = (await cssResource.contentEncoded()) ? window.atob(cssResource.content) : cssResource.content;
  TestRunner.addResult('Resource.content: ' + content);
  TestRunner.completeTest();
})();
