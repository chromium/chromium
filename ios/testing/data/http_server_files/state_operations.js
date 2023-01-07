// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Populates onload text and clear no-op text.
window.onload = function() {
  updateOnLoadText('OnLoadText');
  updateNoOpText('');
};

var updateOnLoadText = function(text) {
  document.getElementById('on-load').innerHTML = text;
}

var updateNoOpText = function(text) {
  document.getElementById('no-op').innerHTML = text;
}

var isOnLoadPlaceholderTextVisible = function() {
  return document.getElementById('on-load').innerHTML == 'OnLoadText';
}

var isNoOpPlaceholderTextVisible = function() {
  return document.getElementById('no-op').innerHTML == 'NoOpText';
}

// When a button is tapped, a 0,5s timeout begins that will call this function.
var onNoOpTimeout = function() {
  document.getElementById('no-op').innerHTML = 'NoOpText';
}

// Updates |gStateParams| to hold the specified values.  These are later used as
// input parameters to history state changes.
var gStateParams = {};
var updateStateParams = function(obj, title, url) {
  gStateParams.obj = obj;
  gStateParams.title = title;
  gStateParams.url = url;
}

// Clears div text so tests can verify that the reported values are the result
// of the latest executed script.
var onButtonTapped = function() {
  updateOnLoadText('');
  updateNoOpText('');
  setTimeout(onNoOpTimeout, 500);
}

var pushStateAction = function() {
  onButtonTapped();
  window.history.pushState(gStateParams.obj, gStateParams.title,
                           gStateParams.url);
}

var replaceStateAction = function() {
  onButtonTapped();
  window.history.replaceState(gStateParams.obj, gStateParams.title,
                              gStateParams.url);
}

