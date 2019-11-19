// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the sidebar uses the correct styling for mixed content subresources.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  var mixedExplanations = [
    {
      securityState: Protocol.Security.SecurityState.Neutral,
      summary: 'Neutral Test Summary',
      description: 'Neutral Test Description',
      mixedContentType: Protocol.Security.MixedContentType.OptionallyBlockable,
      certificate: []
    },
    {
      securityState: Protocol.Security.SecurityState.Insecure,
      summary: 'Insecure Test Summary',
      description: 'Insecure Test Description',
      mixedContentType: Protocol.Security.MixedContentType.Blockable,
      certificate: []
    }
  ];
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Neutral, mixedExplanations, null));

  var passive = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  passive.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(passive);

  var active = new SDK.NetworkRequest(0, 'http://bar.test', 'https://bar.test', 0, 0, null);
  active.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(active);

  TestRunner.addResult('Origin sidebar:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel._instance()._sidebarTree.element);

  TestRunner.completeTest();
})();
