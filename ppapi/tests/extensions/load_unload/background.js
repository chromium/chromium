// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nacl = null;

function loadNaClModule() {
  nacl = document.createElement('embed');
  nacl.addEventListener('load', onNaClModuleLoaded, true);
  nacl.type = 'application/x-nacl';
  nacl.width = 0;
  nacl.height = 0;
  nacl.src = 'ppapi_tests_extensions_load_unload.nmf';
  document.body.appendChild(nacl);

  // Request the offsetTop property to force a relayout. As of Apr 10, 2014
  // this is needed if the module is being loaded in a background page (see
  // crbug.com/350445).
  nacl.offsetTop;
}

function detachNaClModule() {
  document.body.removeChild(nacl);
  nacl = null;
}

function onNaClModuleLoaded() {
  chrome.test.sendMessage("nacl_module_loaded");
}

chrome.browserAction.onClicked.addListener(function(tab) {
  if (!nacl)
    loadNaClModule();
  else
    detachNaClModule();
});
