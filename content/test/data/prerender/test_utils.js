// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adds <link rel=prerender> for the URL.
function add_prerender(url) {
  const link = document.createElement('link');
  link.rel = 'prerender';
  link.href = url;
  document.head.appendChild(link);
}

// Creates a new iframe with the URL.
async function add_iframe(url) {
  const frame = document.createElement('iframe');
  frame.src = url;
  document.body.appendChild(frame);
  return await new Promise(resolve => {
    frame.onload = e => resolve('LOADED');
  });
}

// Opens a new pop-up window with the URL.
async function open_window(url) {
  const win = window.open(url, '_blank');
  if (!win)
    return 'FAILED';
  return await new Promise(resolve => {
    win.onload = e => resolve('LOADED');
  });
}
