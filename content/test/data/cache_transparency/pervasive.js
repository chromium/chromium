// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No-op file for Cache Transparency tests. The only important features of this
// file are that it is cacheable and the checksum matches the expected value.

// When changing this file, you need to update the expected checksum in the
// following files:

// * content/browser/navigation_browsertest.cc
// * third_party/blink/renderer/platform/loader/fetch/resource_loader_test.cc

// You can generate the checksum by running

// python3 ../../../../net/tools/cache_transparency/checksum_pervasive_js.py \
//   pervasive.js
