// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds event listeners and populates on-load-div.
window.onload = function() {
  updateOnLoadText('OnLoadText');
};

var updateOnLoadText = function(text) {
  document.getElementById('on-load-div').innerHTML = text;
}

var updateNoOpText = function() {
  document.getElementById('no-op').innerHTML = 'NoOpText';
}

var buttonWasTapped = function() {
  setTimeout(updateNoOpText, 500);
}

var isOnLoadTextVisible = function() {
  return document.getElementById('on-load-div').innerHTML == 'OnLoadText';
}

var isNoOpTextVisible = function() {
  return document.getElementById('no-op').innerHTML == 'NoOpText';
}

// Updates the url-to-load div with |text|.  This value is later used by the
// window.location calls below.
var updateUrlToLoadText = function(text) {
  document.getElementById('url-to-load').innerHTML = text;
}

// Returns a DOMString representing the URL used as the innerHTML of the
// url-to-load div.
var getUrl = function() {
  return document.getElementById('url-to-load').innerHTML;
}

var locationAssign = function() {
  updateOnLoadText('');
  window.location.assign(getUrl());
  buttonWasTapped();
}

var locationReplace = function() {
  updateOnLoadText('');
  window.location.replace(getUrl());
  buttonWasTapped();
}

var locationReload = function() {
  updateOnLoadText('');
  window.location.reload();
  buttonWasTapped();
}

var setLocationToDOMString = function() {
  updateOnLoadText('');
  window.location = getUrl();
  buttonWasTapped();
}

