// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_

namespace version_info {
enum class Channel;
}

namespace extensions {

// Gets the current channel as seen by the Feature system.
version_info::Channel GetCurrentChannel();

// Sets the current channel as seen by the Feature system. In the browser
// process this should be chrome::GetChannel(), and in the renderer this will
// need to come from an IPC. Note that the value set through this function may
// be overridden by |ScopedCurrentChannel|.
void SetCurrentChannel(version_info::Channel channel);

// Scoped channel setter. Use for tests.
// Note that the lifetimes of multiple instances of this class must be disjoint
// or nested, but never overlapping.
class ScopedCurrentChannel {
 public:
  explicit ScopedCurrentChannel(version_info::Channel channel);

  ScopedCurrentChannel(const ScopedCurrentChannel&) = delete;
  ScopedCurrentChannel& operator=(const ScopedCurrentChannel&) = delete;

  ~ScopedCurrentChannel();

  const version_info::Channel& channel() { return channel_; }

 private:
  const version_info::Channel channel_;
  const version_info::Channel original_overridden_channel_;
  const int original_override_count_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_
