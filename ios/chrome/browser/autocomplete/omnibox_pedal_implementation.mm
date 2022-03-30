// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/omnibox_pedal_implementation.h"

#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/resources/grit/omnibox_pedal_synonyms.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// =============================================================================

class OmniboxPedalClearBrowsingData : public OmniboxPedal {
 public:
  explicit OmniboxPedalClearBrowsingData(bool incognito)
      : OmniboxPedal(
            OmniboxPedalId::CLEAR_BROWSING_DATA,
            LabelStrings(
                IDS_IOS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
                IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
            GURL()),
        incognito_(incognito) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_OPTIONAL_GOOGLE_CHROME
        {
            false,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_OPTIONAL_GOOGLE_CHROME,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_DELETE,
        },
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_CLEAR_BROWSING_DATA_ONE_REQUIRED_INFORMATION,
        },
#endif
    };
  }

  // This method override enables this Pedal to spoof its ID for metrics
  // reporting, making it possible to distinguish incognito usage.
  OmniboxPedalId GetMetricsId() const override {
    return incognito_ ? OmniboxPedalId::INCOGNITO_CLEAR_BROWSING_DATA : id();
  }

 protected:
  ~OmniboxPedalClearBrowsingData() override = default;
  bool incognito_;
};

// =============================================================================

class OmniboxPedalSetChromeAsDefaultBrowser : public OmniboxPedal {
 public:
  explicit OmniboxPedalSetChromeAsDefaultBrowser()
      : OmniboxPedal(
            OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER,
            LabelStrings(
                IDS_IOS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_HINT,
                IDS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUGGESTION_CONTENTS,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_SUFFIX,
                IDS_ACC_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER),
            GURL()) {}

  std::vector<SynonymGroupSpec> SpecifySynonymGroups(
      bool locale_is_english) const override {
    return {
#ifdef IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_ALWAYS_OPEN_LINKS_IN_CHROME
        {
            true,
            true,
            IDS_OMNIBOX_PEDAL_SYNONYMS_SET_CHROME_AS_DEFAULT_BROWSER_ONE_REQUIRED_ALWAYS_OPEN_LINKS_IN_CHROME,
        }
#endif
    };
  }

 protected:
  ~OmniboxPedalSetChromeAsDefaultBrowser() override = default;
};

// =============================================================================

std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
GetPedalImplementations(bool incognito, bool testing) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  __unused const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(std::make_pair(pedal->id(), base::WrapRefCounted(pedal)));
  };

  if (!incognito) {
    add(new OmniboxPedalClearBrowsingData(incognito));
  }

  add(new OmniboxPedalSetChromeAsDefaultBrowser());

  return pedals;
}
