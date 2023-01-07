// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var authToken = '';

function getAuthToken(interactive) {
  chrome.identity.getAuthToken(
      {'interactive': interactive}, onGetAuthToken);
}

function onGetAuthToken(authToken) {
  var signInEl = document.getElementById('signIn');
  var getFileEl = document.getElementById('getFile');
  if (authToken) {
    signInEl.setAttribute('hidden', '');
    getFileEl.removeAttribute('hidden');
    window.authToken =  authToken;

    // Send the auth token to the NaCl module.
    common.naclModule.postMessage('token:'+authToken);
  } else {
    // There is no auth token; this means that the user has yet to authorize
    // this app. Display a button to let the user sign in and authorize this
    // application.
    signInEl.removeAttribute('hidden');
    getFileEl.setAttribute('hidden', '');
  }
};

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();

  // Make sure this example is running as a packaged app. If not, display a
  // warning.
  if (!chrome.identity) {
    common.updateStatus('Error: must be run as a packged app.');
    return;
  }

  // Try to get the authorization token non-interactively. This will often work
  // if the user has already authorized the app, and the token is cached.
  getAuthToken(false);
}

function handleMessage(e) {
  var msg = e.data;
  document.getElementById('contents').textContent = msg;
}

// Called by the common.js module.
function attachListeners() {
  document.getElementById('signIn').addEventListener('click', function () {
    // Get the authorization token interactively. A dialog box will pop up
    // asking the user to authorize access to their Drive account.
    getAuthToken(true);
  });

  document.getElementById('getFile').addEventListener('click', function () {
    // Clear the file contents dialog box.
    document.getElementById('contents').textContent = '';

    common.naclModule.postMessage('getFile');
  });
}
