// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_

#include <string>

#include "base/optional.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Note: if changing this, see also
// content/public/common/common_param_traits_macros.h
struct BLINK_COMMON_EXPORT UserAgentBrandVersion {
  UserAgentBrandVersion() = default;
  UserAgentBrandVersion(const std::string& ua_brand,
                        const std::string& ua_major_version);

  bool operator==(const UserAgentBrandVersion& a) const;

  std::string brand;
  std::string major_version;
};

using UserAgentBrandList = std::vector<UserAgentBrandVersion>;

// Note: if changing this, see also
// content/public/common/common_param_traits_macros.h
struct BLINK_COMMON_EXPORT UserAgentMetadata {
  // Turning the brand list into a structured header comes up often enough and
  // is just non-trivial enough that it's better to be in one place.
  const std::string SerializeBrandVersionList();

  static base::Optional<UserAgentMetadata> Demarshal(
      const base::Optional<std::string>& encoded);
  static base::Optional<std::string> Marshal(
      const base::Optional<UserAgentMetadata>& ua_metadata);
  UserAgentBrandList brand_version_list;

  std::string full_version;
  std::string platform;
  std::string platform_version;
  std::string architecture;
  std::string model;
  bool mobile = false;
};

// Used when customizing the sent User-Agent and Sec-CH-UA-* for
// features like "request desktop site", which override those from defaults
// for some individual navigations. WebContents::SetUserAgentOverride()
// is the main entry point used for the functionality.
//
// Like above, this has legacy IPC traits in
// content/public/common/common_param_traits_macros.h
struct BLINK_COMMON_EXPORT UserAgentOverride {
  // Helper which sets only UA, no client hints.
  static UserAgentOverride UserAgentOnly(const std::string& ua) {
    UserAgentOverride result;
    result.ua_string_override = ua;
    return result;
  }

  // Empty |ua_string_override| means no override;
  // |ua_metadata_override| must also be null in that case.
  std::string ua_string_override;

  // Non-nullopt if custom values for user agent client hint properties
  // should be used. If this is null, and |ua_string_override| is non-empty,
  // no UA client hints will be sent.
  base::Optional<UserAgentMetadata> ua_metadata_override;
};

bool BLINK_COMMON_EXPORT operator==(const UserAgentMetadata& a,
                                    const UserAgentMetadata& b);

bool BLINK_COMMON_EXPORT operator==(const UserAgentOverride& a,
                                    const UserAgentOverride& b);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USER_AGENT_USER_AGENT_METADATA_H_
