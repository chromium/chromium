// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setComponentProcessStateStartedWithNoInstalledComponent() {
  chrome.mediaPerceptionPrivate.setComponentProcessState({
    status: 'STARTED'
  }, chrome.test.callbackPass(function(processState) {
      chrome.test.assertEq({
            status: 'SERVICE_ERROR',
            serviceError: 'SERVICE_NOT_INSTALLED'
          }, processState);
  }));
}

// The component must be installed and the mount point must be set in order to
// start the process.
function setAnalyticsComponentLight() {
  chrome.mediaPerceptionPrivate.setAnalyticsComponent({
    type: 'LIGHT',
  }, chrome.test.callbackPass(function(component_state) {
    chrome.test.assertEq('INSTALLED', component_state.status);
    chrome.test.assertEq('1.0', component_state.version);
  }));
}

function setComponentProcessStateStarted() {
  chrome.mediaPerceptionPrivate.setComponentProcessState({
    status: 'STARTED'
  }, chrome.test.callbackPass(function(processState) {
      chrome.test.assertEq({ status: 'STARTED' }, processState);
  }));
}

function setComponentProcessStateStopped() {
  chrome.mediaPerceptionPrivate.setComponentProcessState({
    status: 'STOPPED'
  }, chrome.test.callbackPass(function(processState) {
      chrome.test.assertEq({ status: 'STOPPED' }, processState);
  }));
}

function setComponentProcessStateUnsettable() {
  const error = 'Cannot set process_state to something other than STARTED or '
      + 'STOPPED.';
  chrome.mediaPerceptionPrivate.setComponentProcessState({
    status: 'UNKNOWN'
  }, chrome.test.callbackFail(error));
  chrome.mediaPerceptionPrivate.setComponentProcessState({
    status: 'SERVICE_ERROR'
  }, chrome.test.callbackFail(error));
}

chrome.test.runTests([
    setComponentProcessStateStartedWithNoInstalledComponent,
    setAnalyticsComponentLight,
    setComponentProcessStateStarted,
    setComponentProcessStateStopped,
    setComponentProcessStateUnsettable]);

