// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var dataArray;
const MIME_TYPE = "application/octet-stream";

function $(id) {
  return document.getElementById(id);
}


function success(stream) {
  track = stream.getVideoTracks()[0];
  var video = $('video')
  video.srcObject = stream;
  video.track = track;
  video.play();

  var list = $('profileList');
  var profile = (list.length < 1) ? 'vp8'
    : list.options[list.selectedIndex].value;

  common.naclModule.postMessage({
    command: 'start',
    track: track,
    profile: profile,
    width: 640,
    height: 480
  });
}

function failure(e) {
  common.logMessage("Error: " + e);
}

function cleanupDownload() {
  var download = $('download');
  if (!download)
    return;
  download.parentNode.removeChild(download);
}

function appendDownload(parent, blob, filename) {
  var a = document.createElement('a');
  a.id = "download";
  a.download = filename;
  a.href = window.URL.createObjectURL(blob);
  a.textContent = 'Download';
  a.dataset.downloadurl = [MIME_TYPE, a.download, a.href].join(':');
  parent.appendChild(a);
}

function startRecord() {
  $('length').innerHTML = ' Size: ' + dataArray.byteLength + ' bytes';
  navigator.webkitGetUserMedia({audio: false, video: true},
                               success, failure);

  $('start').disabled = true;
  $('stop').disabled = false;
  cleanupDownload();
}

function stopRecord() {
  common.naclModule.postMessage({
    command: "stop"
  });
  var video = $('video');
  video.pause();
  video.track.stop();
  video.track = null;

  $('start').disabled = false;
  $('stop').disabled = true;
  appendDownload($('download-box'),
                 new Blob([dataArray], { type: MIME_TYPE }),
                 'Capture.video');
}

function handleMessage(msg) {
  if (msg.data.name == 'data') {
    appendData(msg.data.data);
  } else if (msg.data.name == 'supportedProfiles') {
    common.logMessage('profiles: ' + JSON.stringify(msg.data.profiles));
    var profileList = $('profileList');
    for (var node in profileList.childNodes)
      profileList.remove(node);
    for (var i = 0; i < msg.data.profiles.length; i++) {
      var item = document.createElement('option');
      item.label = item.value = msg.data.profiles[i];
      profileList.appendChild(item);
    }
    $('start').disabled = !(msg.data.profiles.length > 0);
  } else if (msg.data.name == 'log') {
    common.logMessage(msg.data.message);
  }
}

function resetData() {
  dataArray = new ArrayBuffer(0);
}

function appendData(data) {
  var tmp = new Uint8Array(dataArray.byteLength + data.byteLength);
  tmp.set(new Uint8Array(dataArray), 0 );
  tmp.set(new Uint8Array(data), dataArray.byteLength);
  dataArray = tmp.buffer;
  $('length').textContent = ' Size: ' + dataArray.byteLength + ' bytes';
}

function attachListeners() {
  $('start').addEventListener('click', function (e) {
    resetData();
    startRecord();
  });
  $('stop').addEventListener('click', function (e) {
    stopRecord();
  });
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}
