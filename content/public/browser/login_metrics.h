// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LOGIN_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_LOGIN_METRICS_H_

namespace content {

inline constexpr char kBrowserAssistedLoginTypeHistogram[] =
    "PasswordManager.BrowserAssistedLogin.Type";

// This enum describes the type of logins assisted by the browser. e.g. via
// passwords, passkeys or federation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(BrowserAssistedLoginType)
enum class BrowserAssistedLoginType {
  kFedCmPassive = 0,
  kFedCmActive = 1,
  kNonFedCmOAuth = 2,

  kMaxValue = kNonFedCmOAuth
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:BrowserAssistedLoginType)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LOGIN_METRICS_H_
