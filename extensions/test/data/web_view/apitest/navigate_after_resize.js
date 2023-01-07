// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  switch (data[0]) {
    case 'dimension-request':
      var reply = ['dimension-response', window.innerWidth, window.innerHeight];
      embedder.postMessage(JSON.stringify(reply), '*');
      break;
  }
});
