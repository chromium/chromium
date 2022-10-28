// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getUserEmailTest() {
    chrome.feedbackPrivate.getUserEmail(
        chrome.test.callbackPass(function(email) {
      if (email.length > 0) {
        // If we actually have an e-mail address returned, do a simple sanity
        // check on it.
        chrome.test.assertTrue(email.indexOf('@') != -1);
      }
    }));
  },
  function getSystemInfoTest() {
    chrome.feedbackPrivate.getSystemInformation(
        chrome.test.callbackPass(function(sysinfo) {
      // Make sure we get 'some' system info.
      chrome.test.assertTrue(sysinfo.length >= 1);
    }));
  },
  function sendFeedbackTest() {
    var feedbackInfo = {
      description: 'This is a test description',
      sendHistograms: false
    };
    chrome.feedbackPrivate.sendFeedback(
        feedbackInfo, chrome.test.callbackPass(function(status) {
          chrome.test.assertEq(
              chrome.feedbackPrivate.Status.SUCCESS, status);
    }));
  }
]);
