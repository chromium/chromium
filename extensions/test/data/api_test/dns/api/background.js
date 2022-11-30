// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testIPLiteralResolution = function() {
  var callback = function(resolveInfo) {
    chrome.test.assertEq(0, resolveInfo.resultCode);
    chrome.test.assertEq("127.0.0.1", resolveInfo.address);
    chrome.test.succeed("IP literal resolved");
  };
  chrome.dns.resolve("127.0.0.1", callback);
};

var testHostnameResolution = function() {
  var callback = function(resolveInfo) {
    chrome.test.assertEq(0, resolveInfo.resultCode);
    chrome.test.assertEq("9.8.7.6", resolveInfo.address);
    chrome.test.succeed("hostname resolved");
  };
  chrome.dns.resolve("www.sowbug.test", callback);
};

var testNonexistentHostnameResolution = function() {
  var callback = function(resolveInfo) {
    // NET_ERROR(NAME_NOT_RESOLVED, -105)
    chrome.test.assertEq(-105, resolveInfo.resultCode);
    chrome.test.succeed("hostname correctly failed to resolve");
  };
  chrome.dns.resolve("this.hostname.is.bogus.test", callback);
};

chrome.test.runTests([testIPLiteralResolution,
                      testHostnameResolution,
                      testNonexistentHostnameResolution]);
