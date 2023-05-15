// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Notifies the application that this frame has loaded. This file
 * must be included at document end time.
 */

import {registerFrame} from '//ios/web/public/js_messaging/resources/frame_id.js';

// Requires message.js to already have been injected.

// Frame registration must be delayed until Document End script injection time.
// (This file is injected at that time, but the message API is defined at
// Document Start time.)
registerFrame();
