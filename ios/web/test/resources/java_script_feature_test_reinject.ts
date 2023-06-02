// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview Setup used in JavaScriptFeature inttests. This file
 * will be reinjected if the document JS object is modified.
 */

window.addEventListener('error', () => {
  gCrWeb.javaScriptFeatureTest.errorReceivedCount++;
});
