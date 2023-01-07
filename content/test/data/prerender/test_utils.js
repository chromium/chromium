// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates a new iframe with the URL, and returns a promise.
function add_iframe(url) {
  const frame = document.createElement('iframe');
  frame.src = url;
  document.body.appendChild(frame);
  return new Promise(resolve => {
    frame.onload = e => resolve('LOADED');
  });
}

// Creates a new iframe with the URL asynchronously.
const iframe_promises = [];
function add_iframe_async(url) {
  if (iframe_promises[url])
    throw "URL ALREADY USED";
  iframe_promises[url] = add_iframe(url);
}

// Waits until added iframe with the URL finishes loading.
async function wait_iframe_async(url) {
  if (!iframe_promises[url])
    return "URL NOT FOUND";
  const target_promise = iframe_promises[url];
  iframe_promises[url] = null;
  return target_promise;
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

// Returns <iframe> element upon load.
// TODO(nhiroki): Merge this into add_iframe().
function create_iframe(url) {
  return new Promise(resolve => {
      const frame = document.createElement('iframe');
      frame.src = url;
      frame.onload = () => resolve(frame);
      document.body.appendChild(frame);
    });
}

// Returns <img> element upon load.
function create_img(url) {
  return new Promise(resolve => {
      const img = document.createElement('img');
      img.src = url;
      img.onload = () => resolve(img);
      img.onerror = () => resolve(img);
      document.body.appendChild(img);
    });
}
