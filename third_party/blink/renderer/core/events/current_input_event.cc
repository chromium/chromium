// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/current_input_event.h"

namespace blink {

const WebInputEvent* CurrentInputEvent::current_input_event_ = nullptr;

}  // namespace blink
