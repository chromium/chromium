// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_POLICY_HEADLESS_MODE_POLICY_H_
#define HEADLESS_LIB_BROWSER_POLICY_HEADLESS_MODE_POLICY_H_

#include "components/policy/core/browser/configuration_policy_handler.h"  // nogncheck http://crbug.com/1227148
#include "headless/public/headless_export.h"

class PrefService;
class PrefRegistrySimple;

namespace policy {

// Headless mode policy helpers.
class HEADLESS_EXPORT HeadlessModePolicy {
 public:
  // Headless mode as set by policy. The values must match the
  // HeadlessMode policy template in
  // components/policy/resources/policy_templates.json
  enum class HeadlessMode {
    // Headless mode is enabled.
    kEnabled = 1,
    // Headless mode is disabled.
    kDisabled = 2,
    // Default value to ensure consistency.
    kDefaultValue = kEnabled,
    // Min and max values for range checking.
    kMinValue = kEnabled,
    kMaxValue = kDisabled
  };

  // Registers headless mode policy prefs in |registry|.
  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  // Returns the current HeadlessMode policy according to the values in
  // |pref_service|. If no HeadlessMode policy is set, the default will be
  // |HeadlessMode::kEnabled|.
  static HeadlessMode GetPolicy(const PrefService* pref_service);

  // Returns positive if current HeadlessMode policy in |pref_service| is set to
  // |HeadlessMode::kEnabled| or is unset.
  static bool IsHeadlessDisabled(const PrefService* pref_service);
};

#if defined(HEADLESS_MODE_POLICY_SUPPORTED)
// Handles the HeadlessMode policy. Controls the managed values of the
// |kHeadlessMode| pref.
class HeadlessModePolicyHandler : public IntRangePolicyHandler {
 public:
  HeadlessModePolicyHandler();
  ~HeadlessModePolicyHandler() override;

  HeadlessModePolicyHandler(const HeadlessModePolicyHandler&) = delete;
  HeadlessModePolicyHandler& operator=(const HeadlessModePolicyHandler&) =
      delete;
};
#endif

}  // namespace policy

#endif  // HEADLESS_LIB_BROWSER_POLICY_HEADLESS_MODE_POLICY_H_
