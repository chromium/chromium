// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/chrome_root_store_data.h"

namespace net {

namespace {

#include "net/data/ssl/chrome_root_store/chrome-ev-roots-inc.cc"

}  // namespace

base::span<const EVMetadata> GetEvRootCaMetadata() {
  return kEvRootCaMetadata;
}

}  // namespace net
