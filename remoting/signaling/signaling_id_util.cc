// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_id_util.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"

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

std::string GetCanonicalEmail(const std::string& email) {
  DCHECK(email.find('/') == std::string::npos)
      << "This function expects an email address, not a signaling ID.";
  std::string canonical_email = base::ToLowerASCII(email);
  base::TrimString(canonical_email, base::kWhitespaceASCII, &canonical_email);

  size_t at_index = canonical_email.find('@');
  if (at_index == std::string::npos) {
    LOG(ERROR) << "Unexpected email address. Character '@' is missing.";
    return canonical_email;
  }
  std::string username = canonical_email.substr(0, at_index);
  std::string domain = canonical_email.substr(at_index + 1);

  if (domain == kGmailDomain || domain == kGooglemailDomain) {
    // GMail/GoogleMail domains ignore dots, whereas other domains may not.
    base::RemoveChars(username, ".", &username);
    return username + '@' + kGmailDomain;
  }

  return canonical_email;
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

bool IsValidFtlSignalingId(const std::string& signaling_id) {
  std::string email;
  std::string resource;
  if (!SplitSignalingIdResource(signaling_id, &email, &resource)) {
    LOG(ERROR) << "Failed to split signaling id: " << signaling_id;
    return false;
  }
  if (!base::StartsWith(resource, kFtlResourcePrefix)) {
    LOG(ERROR) << "Signaling id resource does not start with a valid prefix: "
               << resource;
    return false;
  }
  std::string registration_id = resource.substr(sizeof(kFtlResourcePrefix) - 1);
  if (!base::Uuid::ParseLowercase(registration_id).is_valid()) {
    LOG(ERROR) << "Signaling id contains an invalid registration id: "
               << registration_id;
    return false;
  }
  return true;
}

}  // namespace remoting
