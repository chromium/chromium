// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function loadScript(src) {
    var s = document.createElement('script');
    s.src = src;
    document.body.appendChild(s);
}

function success() {
    document.title = 'OK';
}

function fail(msg) {
    console.error(msg);
    document.title = 'FAIL';
}

document.title = 'Ready';
