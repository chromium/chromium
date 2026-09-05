// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/email_utils.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/i18n/char_iterator.h"
#include "base/strings/strcat.h"

namespace remoting {

namespace {

constexpr std::u16string_view kEllipsis = u"…";

// Safely truncates |text| to at most |max_len| code units from the start,
// snapping down so as not to split a UTF-16 surrogate pair.
std::u16string_view SafePrefix(std::u16string_view text, size_t max_len) {
  if (text.length() <= max_len) {
    return text;
  }
  size_t pos =
      base::i18n::UTF16CharIterator::LowerBound(text, max_len).array_pos();
  return text.substr(0, pos);
}

// Safely truncates |text| to at most |max_len| code units from the end,
// snapping up so as not to split a UTF-16 surrogate pair.
std::u16string_view SafeSuffix(std::u16string_view text, size_t max_len) {
  if (max_len == 0) {
    return {};
  }
  if (text.length() <= max_len) {
    return text;
  }
  size_t start_pos = text.length() - max_len;
  // If start_pos lands in the middle of a surrogate pair (on a trail
  // surrogate), UpperBound snaps forward to the next character boundary.
  size_t pos =
      base::i18n::UTF16CharIterator::UpperBound(text, start_pos).array_pos();
  return text.substr(pos);
}

// Tail-elides |text| to fit in |budget| code units (including ellipsis).
std::u16string TailElide(std::u16string_view text, size_t budget) {
  if (text.length() <= budget) {
    return std::u16string(text);
  }
  if (budget <= 1) {
    return budget == 0 ? std::u16string() : std::u16string(kEllipsis);
  }
  std::u16string_view prefix = SafePrefix(text, budget - 1);
  return base::StrCat({prefix, kEllipsis});
}

// Middle-elides |text| to fit in |budget| code units (including ellipsis).
// |prefix_weight_numerator| and |prefix_weight_denominator| control the
// fraction of non-ellipsis budget allocated to the prefix.
std::u16string MiddleElide(std::u16string_view text,
                           size_t budget,
                           size_t prefix_weight_numerator = 1,
                           size_t prefix_weight_denominator = 2) {
  if (text.length() <= budget) {
    return std::u16string(text);
  }
  if (budget <= 1) {
    return budget == 0 ? std::u16string() : std::u16string(kEllipsis);
  }
  size_t chars_available = budget - 1;
  size_t prefix_len =
      std::max<size_t>(1, (chars_available * prefix_weight_numerator) /
                              prefix_weight_denominator);
  size_t suffix_len = chars_available - prefix_len;

  std::u16string_view prefix = SafePrefix(text, prefix_len);
  std::u16string_view suffix = SafeSuffix(text, suffix_len);

  return base::StrCat({prefix, kEllipsis, suffix});
}

}  // namespace

std::u16string ElideEmail(std::u16string_view email, size_t max_length) {
  if (email.length() <= max_length) {
    return std::u16string(email);
  }

  if (max_length <= 1) {
    return max_length == 0 ? std::u16string() : std::u16string(kEllipsis);
  }

  const size_t at_pos = email.rfind(u'@');
  // If not a valid email (no '@' or '@' at the ends), or if max_length is too
  // small to split into <user>@<domain> (requires at least 3 characters for
  // 'u@d' or '…@…'), middle-elide the whole string.
  if (at_pos == std::u16string_view::npos || at_pos == 0 ||
      at_pos + 1 == email.length() || max_length <= 2) {
    return MiddleElide(email, max_length, 1, 2);
  }

  std::u16string_view username = email.substr(0, at_pos);
  std::u16string_view domain = email.substr(at_pos + 1);

  // 1 character is reserved for '@'.
  size_t available = max_length - 1;

  // Balanced target: give each half roughly equal base budget.
  size_t target_username = available / 2;
  size_t target_domain = available - target_username;

  size_t username_budget;
  size_t domain_budget;

  if (username.length() <= target_username) {
    // Username is short; give all remaining budget to domain.
    username_budget = username.length();
    domain_budget = available - username_budget;
  } else if (domain.length() <= target_domain) {
    // Domain is short; give all remaining budget to username.
    domain_budget = domain.length();
    username_budget = available - domain_budget;
  } else {
    // Both are long.
    username_budget = target_username;
    domain_budget = target_domain;
  }

  std::u16string elided_username = TailElide(username, username_budget);
  // For the domain, give 1/3 to the prefix and 2/3 to the suffix so the
  // authentic registrable domain / TLD is strongly favored.
  std::u16string elided_domain = MiddleElide(domain, domain_budget, 1, 3);

  return base::StrCat({elided_username, u"@", elided_domain});
}

}  // namespace remoting
