// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

window.addEventListener('pagehide', gCrWeb.findInPage.stop);
