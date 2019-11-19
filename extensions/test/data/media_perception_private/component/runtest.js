// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setAnalyticsComponentLight() {
  chrome.mediaPerceptionPrivate.setAnalyticsComponent({
    type: 'LIGHT',
  }, chrome.test.callbackPass(function(componentState) {
    chrome.test.assertEq('INSTALLED', componentState.status);
    chrome.test.assertEq('1.0', componentState.version);
  }));
}

function setAnalyticsComponentFullExpectFailure() {
  chrome.mediaPerceptionPrivate.setAnalyticsComponent({
    type: 'FULL',
  }, chrome.test.callbackPass(function(componentState) {
    chrome.test.assertEq('FAILED_TO_INSTALL', componentState.status);
    chrome.test.assertEq('NOT_FOUND', componentState.installationErrorCode);
  }));
}

function setAnalyticsComponentWithProcessRunningSuccess() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'RUNNING',
    deviceContext: 'device_context'
  }, chrome.test.callbackPass(function(state) {
    chrome.test.assertEq('RUNNING', state.status);
  }));

  chrome.mediaPerceptionPrivate.setAnalyticsComponent({
    type: 'LIGHT',
  }, chrome.test.callbackPass(function(componentState) {
    chrome.test.assertEq('INSTALLED', componentState.status);
    chrome.test.assertEq('1.0', componentState.version);
  }));
}

chrome.test.runTests([
    setAnalyticsComponentLight,
    setAnalyticsComponentFullExpectFailure,
    setAnalyticsComponentWithProcessRunningSuccess]);

