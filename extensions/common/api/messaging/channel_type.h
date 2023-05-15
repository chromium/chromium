// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_CHANNEL_TYPE_H_
#define EXTENSIONS_COMMON_API_MESSAGING_CHANNEL_TYPE_H_

namespace extensions {

// The type of messaging channel.
enum class ChannelType {
  // A message channel associated with `runtime.sendMessage()` or
  // `tabs.sendMessage()`.
  kSendMessage,
  // A message channel associated with `extension.sendRequest()`.
  kSendRequest,
  // A longer-lived message channel associated with `runtime.connect()`
  // or `tabs.connect()`.
  kConnect,
  // A native message channel. Note that unlike above, both one-time and
  // long-lived native message channels use the same type (because they don't
  // have associated channel names).
  kNative,

  kLast = kNative,
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_CHANNEL_TYPE_H_
