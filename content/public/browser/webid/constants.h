// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_CONSTANTS_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_CONSTANTS_H_

namespace content::webid {

// Keys in 'account' dictionary in accounts endpoint.
inline constexpr char kAccountIdKey[] = "id";
inline constexpr char kAccountEmailKey[] = "email";
inline constexpr char kAccountNameKey[] = "name";
inline constexpr char kAccountPhoneNumberKey[] = "tel";
inline constexpr char kAccountUsernameKey[] = "username";
inline constexpr char kAccountGivenNameKey[] = "given_name";
inline constexpr char kAccountPictureKey[] = "picture";
inline constexpr char kAccountApprovedClientsKey[] = "approved_clients";
inline constexpr char kHintsKey[] = "login_hints";
inline constexpr char kDomainHintsKey[] = "domain_hints";
inline constexpr char kLabelsKey[] = "labels";
inline constexpr char kLabelHintsKey[] = "label_hints";
}  // namespace content::webid

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_CONSTANTS_H_
