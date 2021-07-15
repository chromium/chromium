// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_HELPERS_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_HELPERS_H_

#include <string>

#include "base/values.h"

namespace remoting {

// Parses a serialized JSON message from either the CRD website client or a
// native messaging host instance.  Returns true if the message was successfully
// parsed. If parsing fails, the out params will not be affected.
bool ParseNativeMessageJson(const std::string& message,
                            std::string& message_type,
                            base::Value& parsed_message);

// Creates a response for |request|. Returns a none value if the request is
// malformed.
// For a request like this: {id: "abc", type: "hello"}, the response will be:
// {id: "abc", type: "helloResponse"}.
base::Value CreateNativeMessageResponse(const base::Value& request);

// Adds hello response fields to |response|, which should be created by calling
// CreateNativeMessageResponse(). The supported features field will be absent if
// |supported_features| is none (or default).
void ProcessNativeMessageHelloResponse(
    base::Value& response,
    base::Value supported_features = base::Value());
}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_HELPERS_H_
