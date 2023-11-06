// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_METRICS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_METRICS_H_

// Contains iOS specific password manager metrics definitions and helpers.
namespace password_manager {

// Name of the histogram recording password manager surface visits.
extern const char kPasswordManagerSurfaceVisitHistogramName[];

// Each of the surfaces of the Password Manager
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This must be kept in sync with PasswordManager.SurfaceVisit in enums.xml.
enum class PasswordManagerSurface {
  kPasswordList = 0,      // User opened the main page.
  kPasswordDetails = 1,   // User opened the password details page.
  kPasswordCheckup = 2,   // User opened the password checkup main page.
  kPasswordIssues = 3,    // User opened the password issue list page.
  kPasswordSettings = 4,  // User opened the password settings page.
  kAddPassword = 5,       // User opened the page for adding a credential.
  kMaxValue = kAddPassword,
};

// Records a histogram counting visits to each of the Password Manager surfaces
// after successful authentication (if authentication needed).
void LogPasswordManagerSurfaceVisit(PasswordManagerSurface visited_surface);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_METRICS_IOS_PASSWORD_MANAGER_METRICS_H_
