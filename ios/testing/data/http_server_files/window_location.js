// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateOnLoadText(text) {
  document.getElementById('on-load-div').innerHTML = text;
}

function updateNoOpText() {
  document.getElementById('no-op').innerHTML = 'NoOpText';
}

function buttonWasTapped() {
  setTimeout(updateNoOpText, 500);
}

function isOnLoadTextVisible() {
  return document.getElementById('on-load-div').innerHTML === 'OnLoadText';
}

function isNoOpTextVisible() {
  return document.getElementById('no-op').innerHTML === 'NoOpText';
}

// Updates the url-to-load div with |text|.  This value is later used by the
// window.location calls below.
function updateUrlToLoadText(text) {
  document.getElementById('url-to-load').innerHTML = text;
}

// Returns a DOMString representing the URL used as the innerHTML of the
// url-to-load div.
function getUrl() {
  return document.getElementById('url-to-load').innerHTML;
}

function locationAssign() {
  updateOnLoadText('');
  window.location.assign(getUrl());
  buttonWasTapped();
}

function locationReplace() {
  updateOnLoadText('');
  window.location.replace(getUrl());
  buttonWasTapped();
}

function locationReload() {
  updateOnLoadText('');
  window.location.reload();
  buttonWasTapped();
}

function setLocationToDOMString() {
  updateOnLoadText('');
  window.location = getUrl();
  buttonWasTapped();
}

// Adds event listeners and populates on-load-div.
window.onload = function() {
  updateOnLoadText('OnLoadText');
};
