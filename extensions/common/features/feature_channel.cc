// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_channel.h"

#include <ostream>

#include "base/check_op.h"
#include "components/version_info/version_info.h"

namespace {

// The current channel to be reported, unless overridden by
// |ScopedCurrentChannel|.
version_info::Channel g_current_channel = version_info::Channel::STABLE;

// The current channel overridden by |ScopedCurrentChannel|. The value is valid
// only whenever |g_override_count| is non-zero.
version_info::Channel g_overridden_channel = version_info::Channel::STABLE;

// The number of currently existing instances of |ScopedCurrentChannel|.
int g_override_count = 0;

}  // namespace

namespace extensions {

version_info::Channel GetCurrentChannel() {
  return g_override_count ? g_overridden_channel : g_current_channel;
}

void SetCurrentChannel(version_info::Channel channel) {
  // In certain unit tests, SetCurrentChannel can be called within the same
  // process (where e.g. utility processes run as a separate thread). Don't
  // write if the value is the same to avoid TSAN failures.
  if (channel != g_current_channel)
    g_current_channel = channel;
}

ScopedCurrentChannel::ScopedCurrentChannel(version_info::Channel channel)
    : channel_(channel),
      original_overridden_channel_(g_overridden_channel),
      original_override_count_(g_override_count) {
  g_overridden_channel = channel;
  ++g_override_count;
}

ScopedCurrentChannel::~ScopedCurrentChannel() {
  DCHECK_EQ(original_override_count_ + 1, g_override_count)
      << "Scoped channel setters are not nested properly";
  DCHECK_EQ(g_overridden_channel, channel_)
      << "Scoped channel setters are not nested properly";
  g_overridden_channel = original_overridden_channel_;
  --g_override_count;
}

}  // namespace extensions
