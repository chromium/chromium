// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let socketA;
let socketB;

function openWebSocketConnectionA(urlA) {
  return new Promise(resolve => {
    socketA = new WebSocket(urlA);
    socketA.addEventListener('open', () => resolve(123));
  });
}

function openWebSocketConnectionB(urlB) {
  return new Promise(resolve => {
    socketB = new WebSocket(urlB);
    socketB.addEventListener('open', () => resolve(123));
  });
}

function closeConnection() {
  socketA.close();
}

function isSocketAOpen() {
  return socketA.readyState == 1; // Open
}

function isSocketBOpen() {
  return socketB.readyState == 1; // Open
}