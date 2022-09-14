// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CHANNEL_INFO_H_
#define IOS_CHROME_COMMON_CHANNEL_INFO_H_

#include <string>

namespace version_info {
enum class Channel;
}

// Returns a version string to be displayed in "About chromium" dialog.
std::string GetVersionString();

// Returns a human-readable modifier for the version string. For a branded
// build, this modifier is the channel ("canary", "dev" or "beta" but ""
// for stable). In unbranded builds, the modifier is usually an empty string.
// GetChannelString() is intended to be used for display purpose. To simply
// test the channel, use GetChannel().
std::string GetChannelString();

// Returns the channel for the installation. In branded builds, this will be
// version_info::Channel::{STABLE,BETA,DEV,CANARY}. In unbranded builds, or
// in branded builds when the channel cannot be determined, this will be
// version_info::Channel::UNKNOWN.
version_info::Channel GetChannel();

#endif  // IOS_CHROME_COMMON_CHANNEL_INFO_H_
