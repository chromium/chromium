// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"

#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

SyncLoadResponse::SyncLoadResponse() = default;

SyncLoadResponse::SyncLoadResponse(SyncLoadResponse&& other) = default;

SyncLoadResponse::~SyncLoadResponse() = default;

SyncLoadResponse& SyncLoadResponse::operator=(SyncLoadResponse&& other) =
    default;

}  // namespace blink
