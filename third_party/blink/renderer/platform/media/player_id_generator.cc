// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/player_id_generator.h"

#include <atomic>

namespace blink {

namespace {

std::atomic<int> next_player_id = 0;

}  // namespace

int GetNextMediaPlayerId() {
  return next_player_id++;
}

}  // namespace blink
