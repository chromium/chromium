// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Populates pop-state-received-div and state-object-div upon a popstate event.
function onPopstate(e) {
  updatePopStateReceivedText(true);
  const stateText = e.state ? e.state : '(NO STATE OBJECT)';
  updateStateObjectText(stateText);
}

// Populates hash-change-received-div upon receiving of a hashchange event.
function onHashChange(e) {
  updateHashChangeReceivedText(true);
}

function updateOnLoadText(text) {
  document.getElementById('on-load-div').innerHTML = text;
}

function updateNoOpText(text) {
  document.getElementById('no-op-div').innerHTML = text;
}

function updatePopStateReceivedText(received) {
  const text = received ? 'PopStateReceived' : '';
  document.getElementById('pop-state-received-div').innerHTML = text;
}

function updateStateObjectText(state) {
  document.getElementById('state-object-div').innerHTML = state;
}

function updateHashChangeReceivedText(received) {
  const text = received ? 'HashChangeReceived' : '';
  document.getElementById('hash-change-received-div').innerHTML = text;
}

// Clears all div text an starts a timer that updates no-op-div with "NoOpText"
// after 1s.  This allows tests to verify that no navigations occur after a
// no-op JavaScript call.
function onButtonTapped() {
  updateOnLoadText('');
  updateNoOpText('');
  updatePopStateReceivedText(false);
  updateStateObjectText('');
  updateHashChangeReceivedText(false);
  setTimeout('updateNoOpText(\'NoOpText\')', 1000);
}

function goNoParameter() {
  onButtonTapped();
  window.history.go();
}

function goZero() {
  onButtonTapped();
  window.history.go(0);
}

function goBack() {
  onButtonTapped();
  window.history.back();
}

function goForward() {
  onButtonTapped();
  window.history.forward();
}

function go2() {
  onButtonTapped();
  window.history.go(2);
}

function goBack2() {
  onButtonTapped();
  window.history.go(-2);
}

function pushStateWithHash() {
  onButtonTapped();
  window.history.pushState('STATE_OBJECT', 'Title', '#hash');
}

// Adds event listeners and populates on-load-div.
window.onload = function() {
  window.addEventListener('popstate', onPopstate);
  window.addEventListener('hashchange', onHashChange);
  updateOnLoadText('OnLoadText');
};
