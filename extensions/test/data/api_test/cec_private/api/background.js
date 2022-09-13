// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getInitialState() {
    chrome.cecPrivate.queryDisplayCecPowerState(
        chrome.test.callbackPass(state => {
          chrome.test.assertEq(['on', 'on'], state);
        }));
  },

  function sendStandBy() {
    chrome.cecPrivate.sendStandBy(chrome.test.callbackPass(
        () => chrome.test.sendMessage(
            'standby_call_count',
            chrome.test.callbackPass(
                count => chrome.test.assertEq(1, parseInt(count, 10))))));
  },

  function getStandByStateState() {
    chrome.cecPrivate.queryDisplayCecPowerState(chrome.test.callbackPass(
        state => chrome.test.assertEq(['standby', 'standby'], state)));
  },

  function sendWakeup() {
    chrome.cecPrivate.sendWakeUp(chrome.test.callbackPass(
        () => chrome.test.sendMessage(
            'wakeup_call_count',
            chrome.test.callbackPass(
                count => chrome.test.assertEq(1, parseInt(count, 10))))));
  },

  function getWokeUpState() {
    chrome.cecPrivate.queryDisplayCecPowerState(chrome.test.callbackPass(
        state => chrome.test.assertEq(['on', 'on'], state)));
  }
]);
