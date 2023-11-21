// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

var QueryString = function() {
  // Allows access to query parameters on the URL; e.g., given a URL like:
  //    http://<server>/my.html?test=123&bob=123
  // Parameters can then be accessed via QueryString.test or QueryString.bob.
  var params = {};
  // RegEx to split out values by &.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  function d(s) { return decodeURIComponent(s.replace(/\+/g, ' ')); }
  var match;
  while (match = r.exec(window.location.search.substring(1)))
    params[d(match[1])] = d(match[2]);
  return params;
}();

function sendResult(status) {
  if (window.domAutomationController) {
    window.domAutomationController.send(status);
  } else {
    console.log(status);
  }
}

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

function main() {
  video = document.getElementById('video');

  // Add an error listener to avoid timeouts if playback fails.
  video.addEventListener('error', function(e) {
    logOutput('Video playback failed: ' + e.code + ', "' + e.message + '"');
    sendResult('FAILED');
  }, false);

  video.addEventListener('ended', function(e) {
    logOutput('Test completed');
    sendResult('SUCCESS');
  }, false);

  video.src = QueryString.src;
  video.play();
}