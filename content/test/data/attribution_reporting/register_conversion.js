// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function registerConversion({
  data,
  origin = '',  // use a relative URL for conversion registration
  eventSourceTriggerData,
  priority,
  dedupKey,
} = {}) {
  let img = document.createElement('img');
  img.src = origin + '/server-redirect?.well-known/attribution-reporting' +
      '/trigger-attribution?trigger-data=' + data +
      (eventSourceTriggerData === undefined ?
           '' :
           '&event-source-trigger-data=' + eventSourceTriggerData) +
      (priority === undefined ? '' : '&priority=' + priority) +
      (dedupKey === undefined ? '' : '&dedup-key=' + dedupKey);
  img.onerror = function() {
    document.title = 'converted';
  };
  document.body.appendChild(img);
}

function createTrackingPixel(url) {
  let img = document.createElement('img');
  img.src = url;
  document.body.appendChild(img);
}
