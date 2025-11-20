// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_

#import <map>
#import <memory>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"

class AutocompleteController;
class AutocompleteProviderClient;

/// Keyed Service that owns and manages long-lived Omnibox objects
/// (AutocompleteController, etc.) per presentation context.
class AutocompleteService : public KeyedService {
 public:
  explicit AutocompleteService(
      base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
          client_factory);
  ~AutocompleteService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the AutocompleteController for the given context.
  // If it doesn't exist, it creates one.
  AutocompleteController* GetAutocompleteController(
      OmniboxPresentationContext context);

 private:
  // Creates a new AutocompleteController.
  std::unique_ptr<AutocompleteController> CreateAutocompleteController();

  base::RepeatingCallback<std::unique_ptr<AutocompleteProviderClient>()>
      client_factory_;

  std::map<OmniboxPresentationContext, std::unique_ptr<AutocompleteController>>
      controllers_;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SERVICE_H_
