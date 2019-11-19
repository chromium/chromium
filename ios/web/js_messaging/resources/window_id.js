// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy

// Script to set windowId.
(function() {
// CRWJSWindowIDManager replaces $(WINDOW_ID) with appropriate string upon
// injection.
__gCrWeb['windowId'] = '$(WINDOW_ID)';

// Send messages queued since message.js injection.
__gCrWeb.message.invokeQueues();
}());
