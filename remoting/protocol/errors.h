// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: yuweih - Remove existing usages of this file and delete it.

#ifndef REMOTING_PROTOCOL_ERRORS_H_
#define REMOTING_PROTOCOL_ERRORS_H_

#include "remoting/base/errors.h"

namespace remoting::protocol {

// DEPRECATED: Use `remoting::ErrorCode` instead.
using ErrorCode = remoting::ErrorCode;

// DEPRECATED: Use `remoting::ParseErrorCode` instead.
const auto ParseErrorCode = remoting::ParseErrorCode;

// DEPRECATED: Use `remoting::ErrorCodeToString` instead.
const auto ErrorCodeToString = remoting::ErrorCodeToString;

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ERRORS_H_
