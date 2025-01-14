// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/webnn/ort/buffer_content_ort.h"

namespace webnn::ort {

BufferContentOrt::BufferContentOrt(ScopedOrtValuePtr tensor)
    : tensor_(std::move(tensor)) {}

BufferContentOrt::~BufferContentOrt() = default;

}  // namespace webnn::ort
