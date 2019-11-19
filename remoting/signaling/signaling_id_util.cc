// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_id_util.h"

#include <stddef.h>

#include "base/strings/string_util.h"

namespace remoting {

namespace {

constexpr char kFtlResourcePrefix[] = "chromoting_ftl_";
constexpr char kGmailDomain[] = "gmail.com";
constexpr char kGooglemailDomain[] = "googlemail.com";

}  // namespace

std::string NormalizeSignalingId(const std::string& id) {
  std::string email;
  std::string resource;
  if (SplitSignalingIdResource(id, &email, &resource)) {
    std::string normalized_email = resource.find(kFtlResourcePrefix) == 0
                                       ? GetCanonicalEmail(email)
                                       : base::ToLowerASCII(email);
    return normalized_email + "/" + resource;
  }
  return base::ToLowerASCII(email);
}

std::string GetCanonicalEmail(std::string email) {
  DCHECK(email.find('/') == std::string::npos)
      << "You seemed to pass in a full ID. You should only pass in an email "
      << "address.";
  email = base::ToLowerASCII(email);
  base::TrimString(email, base::kWhitespaceASCII, &email);

  size_t at_index = email.find('@');
  if (at_index == std::string::npos) {
    LOG(ERROR) << "Unexpected email address. Character '@' is missing.";
    return email;
  }
  std::string username = email.substr(0, at_index);
  std::string domain = email.substr(at_index + 1);

  if (domain == kGmailDomain || domain == kGooglemailDomain) {
    // GMail/GoogleMail domains ignore dots, whereas other domains may not.
    base::RemoveChars(username, ".", &username);
    return username + '@' + kGmailDomain;
  }

  return email;
}

bool SplitSignalingIdResource(const std::string& full_id,
                              std::string* email,
                              std::string* resource) {
  size_t slash_index = full_id.find('/');
  if (slash_index == std::string::npos) {
    if (email) {
      *email = full_id;
    }
    if (resource) {
      resource->clear();
    }
    return false;
  }

  if (email) {
    *email = full_id.substr(0, slash_index);
  }
  if (resource) {
    *resource = full_id.substr(slash_index + 1);
  }
  return true;
}

}  // namespace remoting
