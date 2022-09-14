// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function moduleDidLoad() {
  common.naclModule.style.backgroundColor = 'gray';
}

// Called by the common.js module.
function handleMessage(message) {
  common.logMessage(message.data);
}
