// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_USER_AGENT_H_
#define IOS_WEB_COMMON_USER_AGENT_H_

#include <string>

namespace web {

// Enum type specifying a user agent's type.
enum class UserAgentType : short {
  // Used for pages that are generated for app-specific URLs.
  NONE = 0,

  // The default user agent type. The default user agent will be determined by
  // the WebClient.
  AUTOMATIC,

  // Used to specify a mobile browser user agent.
  MOBILE,

  // Used to specify a desktop browser user agent.
  DESKTOP
};

// Returns a string representation of `type`.
std::string GetUserAgentTypeDescription(UserAgentType type);

// Returns a UserAgentType with the given description.  If `description` doesn't
// correspond with a UserAgentType, UserAgentType::NONE will be returned.
UserAgentType GetUserAgentTypeWithDescription(const std::string& description);

// Returns the os cpu info portion for a user agent.
std::string BuildOSCpuInfo();

// Returns the user agent to use for the given product name.
// The returned user agent is very similar to that used by Mobile Safari, for
// web page compatibility.
std::string BuildDesktopUserAgent(const std::string& desktop_product);
std::string BuildMobileUserAgent(const std::string& mobile_product);

}  // namespace web

#endif  // IOS_WEB_COMMON_USER_AGENT_H_
