// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(
      `Tests that the mixed content explanation prompts the user to refresh when there are no recorded requests, and links to the network panel when there are recorded requests.\n`);
  await TestRunner.showPanel('security');

  TestRunner.addResult('\nBefore Refresh --------------');

  const pageVisibleSecurityState = new Security.SecurityModel.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Neutral, null, null,
    ['displayed-mixed-content']);
  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  // At this point, the page has mixed content but no mixed requests have been recorded, so the user should be prompted to refresh.
  var explanations =
      Security.SecurityPanel.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.addResult('\nRefresh --------------');

  // Now simulate a refresh.
  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  var request = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  request.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);
  TestRunner.completeTest();
})();
