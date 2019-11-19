// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the active and pasive mixed content explanations prompt the user to refresh when there are no recorded requests, and link to the network panel when there are recorded requests.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  TestRunner.addResult('\nBefore Refresh --------------');

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

  // At this point, the page has mixed content but no mixed requests have been recorded, so the user should be prompted to refresh.
  var explanations =
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.addResult('\nRefresh --------------');

  // Now simulate a refresh.
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Neutral, mixedExplanations, null));

  var passive = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  passive.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(passive);

  var active = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  active.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(active);

  var explanations =
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);
  TestRunner.completeTest();
})();
