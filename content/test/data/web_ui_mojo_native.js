// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

window.isNativeMojoAvailable = () => {
  return 'Mojo' in self && 'MojoHandle' in self && 'MojoWatcher' in self &&
      'createMessagePipe' in Mojo && 'writeMessage' in MojoHandle.prototype &&
      'cancel' in MojoWatcher.prototype;
};

window.isChromeSendAvailable = () => {
  return 'chrome' in self && 'send' in chrome;
};
