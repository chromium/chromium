// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_FEATURES_H_

class PrefService;

// Whether the user should see the import from Safari workflow according to
// their profile prefs.
bool ShouldShowSafariDataImportEntryPoint(PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_MODEL_FEATURES_H_
