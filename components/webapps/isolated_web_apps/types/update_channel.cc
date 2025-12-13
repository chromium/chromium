// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/types/update_channel.h"

#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace web_app {

UpdateChannel::UpdateChannel(std::string channel_name)
    : name_(std::move(channel_name)) {}

UpdateChannel::UpdateChannel(const UpdateChannel&) = default;

UpdateChannel::UpdateChannel(UpdateChannel&&) = default;

UpdateChannel& UpdateChannel::operator=(const UpdateChannel&) = default;

UpdateChannel& UpdateChannel::operator=(UpdateChannel&&) = default;

UpdateChannel::~UpdateChannel() = default;

bool UpdateChannel::operator==(const UpdateChannel& other) const = default;
auto UpdateChannel::operator<=>(const UpdateChannel& other) const = default;
bool UpdateChannel::operator<(const UpdateChannel& other) const = default;

// static
const UpdateChannel& UpdateChannel::default_channel() {
  static const base::NoDestructor<UpdateChannel> kDefaultChannel(
      [] { return *UpdateChannel::Create("default"); }());
  return *kDefaultChannel;
}

// static
base::expected<UpdateChannel, std::monostate> UpdateChannel::Create(
    std::string input) {
  if (input.empty() || !base::IsStringUTF8(input)) {
    return base::unexpected(std::monostate());
  }
  return UpdateChannel(std::move(input));
}

void PrintTo(const UpdateChannel& channel, std::ostream* ostr) {
  *ostr << channel.ToString();
}

}  // namespace web_app
