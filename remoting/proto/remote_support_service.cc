// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/remote_support_service.h"

namespace remoting::internal {

RemoteSupportHostStruct::RemoteSupportHostStruct() = default;
RemoteSupportHostStruct::~RemoteSupportHostStruct() = default;
RemoteSupportHostStruct::RemoteSupportHostStruct(
    const RemoteSupportHostStruct&) = default;
RemoteSupportHostStruct::RemoteSupportHostStruct(RemoteSupportHostStruct&&) =
    default;
RemoteSupportHostStruct& RemoteSupportHostStruct::operator=(
    const RemoteSupportHostStruct&) = default;
RemoteSupportHostStruct& RemoteSupportHostStruct::operator=(
    RemoteSupportHostStruct&&) = default;

}  // namespace remoting::internal
