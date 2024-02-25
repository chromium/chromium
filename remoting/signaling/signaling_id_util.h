// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_ID_UTIL_H_
#define REMOTING_SIGNALING_SIGNALING_ID_UTIL_H_

#include <string>

namespace remoting {

// Normalizes |id|. If |id| is an FTL ID then the email part will be
// canonicalized. Otherwise it will simply convert case-insensitive parts (node
// and domain) to lower-case.
std::string NormalizeSignalingId(const std::string& id);

// Returns the canonical email for the given email. Note that this only works
// for email address and does not work for full signaling ID.
//
// Canonicalizes by:
//   * changing to lowercase
//   * removing all dots if this is a gmail.com or googlemail.com domain
//   * normalize email domain googlemail.com to gmail.com
std::string GetCanonicalEmail(const std::string& email);

// Splits a signaling ID into a the email and a resource suffix.  Either
// |full_id|, |resource|, or both may be null.  If |full_id| is already an email
// address, |resource| is set to the empty string.  Returns true if |full_id|
// has a resource, false if not.
//
// e.g. "user@domain/resource" -> "user@domain", "resource", true
//      "user@domain"          -> "user@domain", "",         false
bool SplitSignalingIdResource(const std::string& full_id,
                              std::string* email,
                              std::string* resource);

// Returns whether |signaling_id| represents a valid FTL signaling ID.
bool IsValidFtlSignalingId(const std::string& signaling_id);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_ID_UTIL_H_
