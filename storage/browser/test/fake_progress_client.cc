// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_progress_client.h"

namespace storage {

void FakeProgressClient::OnProgress(uint64_t delta) {
  total_size += delta;
  call_count++;
}

}  // namespace storage
