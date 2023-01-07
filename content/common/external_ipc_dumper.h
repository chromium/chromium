// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_EXTERNAL_IPC_DUMPER_H_
#define CONTENT_COMMON_EXTERNAL_IPC_DUMPER_H_

#include "ipc/ipc_channel_proxy.h"

namespace content {

IPC::ChannelProxy::OutgoingMessageFilter* LoadExternalIPCDumper(
    const base::FilePath& dump_directory);

}  // namespace content

#endif  // CONTENT_COMMON_EXTERNAL_IPC_DUMPER_H_
