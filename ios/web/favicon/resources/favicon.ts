// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Gets and returns favicons in main frame. This script
 * should be injected only into the main frame when it's loaded.
 */

import { sendFaviconUrls } from "//ios/web/favicon/resources/favicon_utils.js";

sendFaviconUrls();