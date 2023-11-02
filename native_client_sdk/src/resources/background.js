// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function makeURL(toolchain, config) {
  return 'index.html?tc=' + toolchain + '&config=' + config;
}

function createWindow(url) {
  console.log('loading ' + url);
  chrome.app.window.create(url, {
    width: 1024,
    height: 800,
    frame: 'none'
  });
}

function onLaunched(launchData) {
  // Send and XHR to get the URL to load from a configuration file.
  // Normally you won't need to do this; just call:
  //
  // chrome.app.window.create('<your url>', {...});
  //
  // In the SDK we want to be able to load different URLs (for different
  // toolchain/config combinations) from the commandline, so we to read
  // this information from the file "run_package_config".
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'run_package_config', true);
  xhr.onload = function() {
    var toolchain_config = this.responseText.split(' ');
    createWindow(makeURL.apply(null, toolchain_config));
  };
  xhr.onerror = function() {
    // Can't find the config file, just load the default.
    createWindow('index.html');
  };
  xhr.send();
}

chrome.app.runtime.onLaunched.addListener(onLaunched);
