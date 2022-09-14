// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  window.addEventListener('popstate', onPopstate);
  updateOnloadDivText('onload');
  pollForURLAndStateChanges();
};

var onPopstate = function(e) {
  var state = e.state;
  var displayText = !state ? '' : state.button || state;
  updateStatusText(displayText);
  // Only clear the onload text for popstate events.  Cross-document navigations
  // to pages with state objects should display both onload and the state.
  if (displayText)
    clearOnloadDivText();
};

var updateOnloadDivText = function(text) {
  document.getElementById('onloaddiv').innerHTML = text;
};

var clearOnloadDivText = function() {
  updateOnloadDivText('');
};

var pollForURLAndStateChanges = function() {
  document.getElementById('currentUrl').innerHTML = window.location.href;
  var state = window.history.state;
  var displayText = !state ? '' : state.button || state;
  updateStatusText(displayText);
  window.setTimeout(pollForURLAndStateChanges, 50);
};

var updateStatusText = function(text) {
  if (!text)
    document.getElementById('status').innerHTML = '';
  else {
    document.getElementById('status').innerHTML = 'Action: ' + text;
  }
};

function pushStateHashWithObject() {
  clearOnloadDivText();
  var state = {
    button: 'pushStateHashWithObject',
    a: new Date(),
    b: 1,
    c: true
  };
  window.history.pushState(state, 'Push with object', '#pushWithObject');
  updateStatusText('pushStateHashWithObject');
};

function pushStateHashString() {
  clearOnloadDivText();
  window.history.pushState(
      'pushStateHashString',
      'Push hash string',
      '#string');
  updateStatusText('pushStateHashString');
};

function pushStateRootPath() {
  clearOnloadDivText();
  window.history.pushState(
      'pushStateRootPath', 'Push root path', '/ios/rootpath');
  updateStatusText('pushStateRootPath');
};

function pushStatePath() {
  clearOnloadDivText();
  window.history.pushState('pushStatePath', 'Push path', 'path');
  updateStatusText('pushStatePath');
};

function pushStatePathSpace() {
  clearOnloadDivText();
  window.history.pushState('pushStatePathSpace', 'Push path space', 'pa th');
  updateStatusText('pushStatePathSpace');
};

function replaceStateHashWithObject() {
  clearOnloadDivText();
  var state = {
    button: "replaceStateHashWithObject",
    a: [1, 2, 3, 4, 5],
    b: ['foo', false, 'bar', 3.6]
  };
  window.history.replaceState(
      state,
      'Replace with object',
      '#replaceWithObject');
  updateStatusText('replaceStateHashWithObject');
};

function replaceStateHashString() {
  clearOnloadDivText();
  window.history.replaceState(
      'replaceStateHashString', 'Replace state hash', '#replace');
  updateStatusText('replaceStateHashString');
};

function replaceStateRootPathSpace() {
  clearOnloadDivText();
  window.history.replaceState(
      'replaceStateRootPathSpace',
      'Replace root path',
      '/ios/rep lace');
  updateStatusText('replaceStateRootPathSpace');
};

function replaceStatePath() {
  clearOnloadDivText();
  window.history.replaceState('replaceStatePath', 'Replace path', 'replace');
  updateStatusText('replaceStatePath');
};

function replaceStateThenPushState() {
  clearOnloadDivText();
  window.history.replaceState('firstReplaceState', 'First replaceState',
      '#firstReplaceState');
  window.history.pushState('replaceStateThenPushState',
      'Replace state then push state', '#replaceStateThenPushState');
  updateStatusText('replaceStateThenPushState');
}

function pushStateThenReplaceState() {
  clearOnloadDivText();
  window.history.pushState('firstPushState', 'First pushState',
      '#firstPushState');
  window.history.replaceState('pushStateThenReplaceState',
      'Push state then replace state', '#pushStateThenReplaceState');
  updateStatusText('pushStateThenReplaceState');
}

function goBack() {
  clearOnloadDivText();
  window.history.back();
};

function goBack2() {
  clearOnloadDivText();
  window.history.go(-2);
};

function goBack3() {
  clearOnloadDivText();
  window.history.go(-3);
};

function goBack4() {
  clearOnloadDivText();
  window.history.go(-4);
};

function goForward() {
  clearOnloadDivText();
  window.history.forward();
};

function goForward2() {
  clearOnloadDivText();
  window.history.go(2);
};

function goForward4() {
  clearOnloadDivText();
  window.history.go(4);
};

function pushStateUnicode() {
  var unicodeChar = String.fromCharCode(0x1111);
  clearOnloadDivText();
  window.history.pushState(
      'pushStateUnicode' + unicodeChar,
      'Push unicode',
      '#unicode' + unicodeChar);
  updateStatusText('pushStateUnicode' + unicodeChar);
};

function pushStateUnicode2() {
  var unicodeChar = String.fromCharCode(0x2222);
  clearOnloadDivText();
  var state = {
    button: "pushStateUnicode2" + unicodeChar,
    title: "Push unicode 2" + unicodeChar
  };
  window.history.pushState(
      state,
      '',
      '#unicode2' + unicodeChar);
  updateStatusText('pushStateUnicode2' + unicodeChar);
};
