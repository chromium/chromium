// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds event listeners and populates on-load-div.
window.onload = function() {
  window.addEventListener('popstate', onPopstate);
  window.addEventListener('hashchange', onHashChange);
  updateOnLoadText('OnLoadText');
};

// Populates pop-state-received-div and state-object-div upon a popstate event.
var onPopstate = function(e) {
  updatePopStateReceivedText(true);
  var stateText = e.state ? e.state : '(NO STATE OBJECT)';
  updateStateObjectText(stateText);
};

// Populates hash-change-received-div upon receiving of a hashchange event.
var onHashChange = function(e) {
  updateHashChangeReceivedText(true);
}

var updateOnLoadText = function(text) {
  document.getElementById('on-load-div').innerHTML = text;
}

var updateNoOpText = function(text) {
  document.getElementById('no-op-div').innerHTML = text;
}

var updatePopStateReceivedText = function(received) {
  var text = received ? 'PopStateReceived' : '';
  document.getElementById('pop-state-received-div').innerHTML = text;
}

var updateStateObjectText = function(state) {
  document.getElementById('state-object-div').innerHTML = state;
}

var updateHashChangeReceivedText = function(received) {
  var text = received ? 'HashChangeReceived' : '';
  document.getElementById('hash-change-received-div').innerHTML = text;
}

// Clears all div text an starts a timer that updates no-op-div with "NoOpText"
// after 1s.  This allows tests to verify that no navigations occur after a
// no-op JavaScript call.
var onButtonTapped = function() {
  updateOnLoadText('');
  updateNoOpText('');
  updatePopStateReceivedText(false);
  updateStateObjectText('');
  updateHashChangeReceivedText(false);
  setTimeout("updateNoOpText('NoOpText')", 1000);
}

var goNoParameter = function() {
  onButtonTapped();
  window.history.go();
}

var goZero = function() {
  onButtonTapped();
  window.history.go(0);
}

var goBack = function() {
  onButtonTapped();
  window.history.back();
}

var goForward = function() {
  onButtonTapped();
  window.history.forward();
}

var go2 = function() {
  onButtonTapped();
  window.history.go(2);
}

var goBack2 = function() {
  onButtonTapped();
  window.history.go(-2);
}

var pushStateWithHash = function() {
  onButtonTapped();
  window.history.pushState('STATE_OBJECT', 'Title', '#hash');
}
