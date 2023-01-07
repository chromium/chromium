// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

aM = null;

function moduleLoad() {
  hideStatus();
  init();
  animate();
  NaClAMBulletInit();
  loadJenga20();
}

function moduleLoadError() {
  updateStatus('Load failed.');
}

function moduleLoadProgress(event) {
  $('progress').style.display = 'block';

  var loadPercent = 0.0;
  var bar = $('progress-bar');

  if (event.lengthComputable && event.total > 0) {
    loadPercent = event.loaded / event.total * 100.0;
  } else {
    // The total length is not yet known.
    loadPercent = 10;
  }
  bar.style.width = loadPercent + "%";
}

function moduleCrash(event) {
  if (naclModule.exitStatus == -1) {
    updateStatus('CRASHED');
  } else {
    updateStatus('EXITED [' + naclModule.exitStatus + ']');
  }
}

function updateStatus(opt_message) {
  var statusField = $('statusField');
  if (statusField) {
    statusField.style.display = 'block';
    statusField.textContent = opt_message;
  }
}

function hideStatus() {
  $('loading-cover').style.display = 'none';
}

function pageDidLoad() {
  updateStatus('Loading...');
  console.log('started');

  aM = new NaClAM('NaClAM');
  aM.enable();

  var embedWrap = $('listener');
  embedWrap.addEventListener('load', moduleLoad, true);
  embedWrap.addEventListener('error', moduleLoadError, true);
  embedWrap.addEventListener('progress', moduleLoadProgress, true);
  embedWrap.addEventListener('crash', moduleCrash, true);

  var revision = 236779;
  var url = '//storage.googleapis.com/gonacl/demos/publish/' +
      revision + '/bullet/NaClAMBullet.nmf';

  var embed = document.createElement('embed');
  embed.setAttribute('name', 'NaClAM');
  embed.setAttribute('id', 'NaClAM');
  embed.setAttribute('width', '0');
  embed.setAttribute('height', '0');
  embed.setAttribute('type', 'application/x-pnacl');
  embed.setAttribute('src', url);
  embedWrap.appendChild(embed);

}

window.addEventListener("load", pageDidLoad, false);
