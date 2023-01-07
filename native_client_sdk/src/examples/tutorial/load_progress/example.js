// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  var listener = document.getElementById('listener');
  listener.addEventListener('loadstart', moduleDidStartLoad, true);
  listener.addEventListener('progress', moduleLoadProgress, true);
  listener.addEventListener('error', moduleLoadError, true);
  listener.addEventListener('abort', moduleLoadAbort, true);
  listener.addEventListener('load', moduleDidLoad, true);
  listener.addEventListener('loadend', moduleDidEndLoad, true);
  listener.addEventListener('message', handleMessage, true);

  common.createNaClModule(name, tc, config, width, height);
}

// Handler that gets called when the NaCl module starts loading.  This
// event is always triggered when an <EMBED> tag has a MIME type of
// application/x-nacl.
function moduleDidStartLoad() {
  common.logMessage('loadstart');
}

// Progress event handler.  |event| contains a couple of interesting
// properties that are used in this example:
//     total The size of the NaCl module in bytes.  Note that this value
//         is 0 until |lengthComputable| is true.  In particular, this
//         value is 0 for the first 'progress' event.
//     loaded The number of bytes loaded so far.
//     lengthComputable A boolean indicating that the |total| field
//         represents a valid length.
//
// event The ProgressEvent that triggered this handler.
function moduleLoadProgress(event) {
  var loadPercent = 0.0;
  var loadPercentString;
  if (event.lengthComputable && event.total > 0) {
    loadPercent = event.loaded / event.total * 100.0;
    loadPercentString = loadPercent + '%';
    common.logMessage('progress: ' + event.url + ' ' + loadPercentString +
                     ' (' + event.loaded + ' of ' + event.total + ' bytes)');
  } else {
    // The total length is not yet known.
    common.logMessage('progress: Computing...');
  }
}

// Handler that gets called if an error occurred while loading the NaCl
// module.  Note that the event does not carry any meaningful data about
// the error, you have to check lastError on the <EMBED> element to find
// out what happened.
function moduleLoadError() {
  common.logMessage('error: ' + common.naclModule.lastError);
}

// Handler that gets called if the NaCl module load is aborted.
function moduleLoadAbort() {
  common.logMessage('abort');
}

// When the NaCl module has loaded indicate success.
function moduleDidLoad() {
  common.logMessage('load');
  common.updateStatus('LOADED');
}

// Handler that gets called when the NaCl module loading has completed.
// You will always get one of these events, regardless of whether the NaCl
// module loaded successfully or not.  For example, if there is an error
// during load, you will get an 'error' event and a 'loadend' event.  Note
// that if the NaCl module loads successfully, you will get both a 'load'
// event and a 'loadend' event.
function moduleDidEndLoad() {
  common.logMessage('loadend');
  var lastError = event.target.lastError;
  if (lastError == undefined || lastError.length == 0) {
    lastError = '<none>';
  }
  common.logMessage('lastError: ' + lastError);
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  common.logMessage('Received PostMessage: ' + message_event.data);
}
