// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_TRACING_HELPERS_H_
#define IPC_TRACING_HELPERS_H_

#include <string_view>

#include "ipc/tracing_helpers_internal.h"

namespace ipc {

// Calculates the first 32 bits of the MD5 digest of the provided data. Used for
// calculating an ID used for tracing legacy IPC messages. Do not use this in
// new code.
constexpr uint32_t GetLegacyIpcTraceId(std::string_view string);

}  // namespace ipc

#endif  // IPC_TRACING_HELPERS_H_
