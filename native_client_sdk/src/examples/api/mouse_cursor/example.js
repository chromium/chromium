// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setCursorToSelectValue() {
  var selectEl = document.querySelector('select');
  var cursorType = parseInt(selectEl.value, 10);
  common.naclModule.postMessage(cursorType);
}

function moduleDidLoad() {
  setCursorToSelectValue();
}

function attachListeners() {
  var selectEl = document.querySelector('select');
  selectEl.addEventListener('change', function (e) {
    setCursorToSelectValue();
  });
}
