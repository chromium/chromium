// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CONSTANTS_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CONSTANTS_H_

namespace remoting {

// ID used to identify the current message. Must be included in the response if
// the sender includes it.
extern const char kMessageId[];

// The type of the message received. The type is used to retrieve and validate
// the message payload.
extern const char kMessageType[];

// Initial message sent from the client to the host to request the host's
// version and supported features. It has no parameters.
extern const char kHelloMessage[];
// Hello response parameters.
extern const char kHostVersion[];
extern const char kSupportedFeatures[];
// Response sent back to the client after the Hello message has been handled.
extern const char kHelloResponse[];

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CONSTANTS_H_
