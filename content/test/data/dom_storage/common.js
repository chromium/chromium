// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function debug(message) {
  var span = document.createElement("span");
  span.appendChild(document.createTextNode(message));
  span.appendChild(document.createElement("br"));
  document.getElementById('status').appendChild(span);
}

function done(message) {
  if (document.location.hash == '#fail')
    return;
  if (message)
    debug('PASS: ' + message);
  else
    debug('PASS');
  document.location.hash = '#pass';
}

function fail(message) {
  debug('FAILED: ' + message);
  document.location.hash = '#fail';
}

function getLog() {
  return "" + document.getElementById('status').innerHTML;
}
