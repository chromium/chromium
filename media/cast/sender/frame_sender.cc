// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/frame_sender.h"

namespace media::cast {

FrameSender::Client::~Client() = default;

FrameSender::FrameSender() = default;
FrameSender::~FrameSender() = default;

}  // namespace media::cast
