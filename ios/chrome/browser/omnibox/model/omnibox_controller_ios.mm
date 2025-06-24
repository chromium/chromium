// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram.h"
#import "base/strings/utf_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller_emitter.h"
#import "components/omnibox/browser/autocomplete_enums.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_popup_view.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "components/search_engines/template_url_starter_pack_data.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ui/gfx/geometry/rect.h"

OmniboxControllerIOS::OmniboxControllerIOS(
    OmniboxClient* client,
    base::TimeDelta autocomplete_stop_timer_duration)
    : autocomplete_controller_(std::make_unique<AutocompleteController>(
          client->CreateAutocompleteProviderClient(),
          AutocompleteClassifier::DefaultOmniboxProviders(),
          autocomplete_stop_timer_duration)) {
  // Register the `AutocompleteController` with `AutocompleteControllerEmitter`.
  if (auto* emitter = client->GetAutocompleteControllerEmitter()) {
    autocomplete_controller_->AddObserver(emitter);
  }
}

OmniboxControllerIOS::~OmniboxControllerIOS() = default;
