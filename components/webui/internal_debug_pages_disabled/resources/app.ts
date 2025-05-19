// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

document.addEventListener('DOMContentLoaded', () => {
  if (!loadTimeData.getBoolean('debugPagesEnabled')) {
    return;
  }

  const host = loadTimeData.getString('host');
  if (host) {
    window.location.href = `chrome://${host}`;
  }
});
