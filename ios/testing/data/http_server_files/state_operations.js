// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateOnLoadText(text) {
  document.getElementById('on-load').innerHTML = text;
}

function updateNoOpText(text) {
  document.getElementById('no-op').innerHTML = text;
}

function isOnLoadPlaceholderTextVisible() {
  return document.getElementById('on-load').innerHTML === 'OnLoadText';
}

function isNoOpPlaceholderTextVisible() {
  return document.getElementById('no-op').innerHTML === 'NoOpText';
}

// When a button is tapped, a 0,5s timeout begins that will call this function.
function onNoOpTimeout() {
  document.getElementById('no-op').innerHTML = 'NoOpText';
}

// Updates |gStateParams| to hold the specified values.  These are later used as
// input parameters to history state changes.
const gStateParams = {};
function updateStateParams(obj, title, url) {
  gStateParams.obj = obj;
  gStateParams.title = title;
  gStateParams.url = url;
}

// Clears div text so tests can verify that the reported values are the result
// of the latest executed script.
function onButtonTapped() {
  updateOnLoadText('');
  updateNoOpText('');
  setTimeout(onNoOpTimeout, 500);
}

function pushStateAction() {
  onButtonTapped();
  window.history.pushState(gStateParams.obj, gStateParams.title,
                           gStateParams.url);
}

function replaceStateAction() {
  onButtonTapped();
  window.history.replaceState(gStateParams.obj, gStateParams.title,
                              gStateParams.url);
}

// Populates onload text and clear no-op text.
window.onload = function() {
  updateOnLoadText('OnLoadText');
  updateNoOpText('');
};
