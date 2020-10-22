// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/sync_load_response.h"

namespace blink {

SyncLoadResponse::SyncLoadResponse() = default;

SyncLoadResponse::SyncLoadResponse(SyncLoadResponse&& other) = default;

SyncLoadResponse::~SyncLoadResponse() = default;

SyncLoadResponse& SyncLoadResponse::operator=(SyncLoadResponse&& other) =
    default;

}  // namespace blink
