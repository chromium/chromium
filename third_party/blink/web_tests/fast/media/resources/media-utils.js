// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function log(m) {
  document.getElementById('results').innerHTML += m + '<br>';
}

function logMatchMediaQuery(query) {
  log(`Query "${query}": ${window.matchMedia(query).matches}`);
}