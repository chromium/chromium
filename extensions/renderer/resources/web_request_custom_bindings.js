// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webRequest API.

if (!apiBridge) {
  var binding = require('binding').Binding.create('webRequest');
  var webRequestEvent = require('webRequestEvent').WebRequestEvent;
  binding.registerCustomEvent(webRequestEvent);
  exports.$set('binding', binding.generate());
}
