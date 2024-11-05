// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_
#define NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_

namespace net {

// The reason why multiplexed session was created. It is used to distinguish
// between preconnect initiated session and other sessions.
// TODO(crbug.com/376304027): Add more precise reasons why preconnect happened.
enum class MultiplexedSessionCreationInitiator {
  kUnknown,
  kPreconnect,
};

}  // namespace net

#endif  // NET_SPDY_MULTIPLEXED_SESSION_CREATION_INITIATOR_H_
