// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/mock_log.h"

namespace audio {

MockLog::MockLog() : receiver_(this) {}

MockLog::~MockLog() = default;

}  // namespace audio
