// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/connect_to_host_info.h"

namespace remoting {

ConnectToHostInfo::ConnectToHostInfo() = default;
ConnectToHostInfo::ConnectToHostInfo(const ConnectToHostInfo& other) = default;
ConnectToHostInfo::ConnectToHostInfo(ConnectToHostInfo&& other) = default;
ConnectToHostInfo::~ConnectToHostInfo() = default;

ConnectToHostInfo& ConnectToHostInfo::operator=(
    const ConnectToHostInfo& other) = default;

}  // namespace remoting
