// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function simulateClick(target) {
  simulateClickWithButton(target, 0 /* left click */);
}

function simulateMiddleClick(target) {
  simulateClickWithButton(target, 1 /* middle click */);
}

function simulateClickWithButton(target, button) {
  if (typeof target === 'string')
    target = document.getElementById(target);

  let evt = new MouseEvent('click', {'button': button});
  return target.dispatchEvent(evt);
}
