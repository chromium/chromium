// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_object_impl.h"

namespace webnn {

WebNNObjectImpl::WebNNObjectImpl(const base::UnguessableToken& handle)
    : handle_(handle) {}

WebNNObjectImpl::~WebNNObjectImpl() = default;

}  // namespace webnn
