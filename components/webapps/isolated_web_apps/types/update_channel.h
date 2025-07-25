// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHANNEL_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHANNEL_H_

#include <string>
#include <variant>

#include "base/types/expected.h"

namespace web_app {

class UpdateChannel {
 public:
  // Returns an instance of the "default" update channel.
  static const UpdateChannel& default_channel();

  static base::expected<UpdateChannel, std::monostate> Create(
      std::string input);

  UpdateChannel(const UpdateChannel&);
  UpdateChannel(UpdateChannel&&);
  UpdateChannel& operator=(const UpdateChannel&);
  UpdateChannel& operator=(UpdateChannel&&);

  ~UpdateChannel();

  bool operator==(const UpdateChannel& other) const;
  auto operator<=>(const UpdateChannel& other) const;
  bool operator<(const UpdateChannel& other) const;

  const std::string& ToString() const { return name_; }

  // For gtest
  friend void PrintTo(const UpdateChannel& channel, std::ostream* ostr);

 private:
  explicit UpdateChannel(std::string channel_name);

  std::string name_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_UPDATE_CHANNEL_H_
