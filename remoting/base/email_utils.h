// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_EMAIL_UTILS_H_
#define REMOTING_BASE_EMAIL_UTILS_H_

#include <string>
#include <string_view>

namespace remoting {

// Default maximum length for elided emails in UI displays.
inline constexpr size_t kDefaultMaxEmailLength = 36;

// Elides an email address to fit within |max_length| characters while
// preserving key identifying details:
// - The local part (username) is tail-elided (e.g. "remote.collaborator…") to
//   preserve the leading characters that recognize the user.
// - The domain is middle-elided (e.g. "support…untrusted.tld") to preserve both
//   the domain prefix and the authoritative registrable domain / TLD suffix,
//   mitigating domain impersonation and spoofing.
// - Budget is dynamically balanced: if one part is shorter than its share, the
//   remainder is given to the other part so unnecessary truncation is avoided.
//
// If |email| does not contain an '@' separator, it is middle-elided.
//
// The ellipsis character used is the Unicode horizontal ellipsis (U+2026, "…").
std::u16string ElideEmail(std::u16string_view email,
                          size_t max_length = kDefaultMaxEmailLength);

}  // namespace remoting

#endif  // REMOTING_BASE_EMAIL_UTILS_H_
