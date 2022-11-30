// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_
#define REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_

#include <string>

namespace remoting {

enum class It2MeHostState;

// Provides a human readable name for a given It2MeHostState. This is used both
// for logging and in host state changed JSON messages.
std::string It2MeHostStateToString(It2MeHostState host_state);

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HELPERS_H_
