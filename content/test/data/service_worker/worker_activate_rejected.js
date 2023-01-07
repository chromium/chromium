// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onactivate = function(event) {
    event.waitUntil(new Promise(function(resolve, reject) {
        setTimeout(reject, 5);
    }));
};
