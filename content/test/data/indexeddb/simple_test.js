// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  indexedDBTest(storeConnection);
}

function storeConnection() {
  db = event.target.result;
}
