// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_

#import <memory>

#import "base/compiler_specific.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/search_engines/template_url_starter_pack_data.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

class OmniboxClient;

// This class controls the various services that can modify the content of the
// omnibox, including `AutocompleteController` and `OmniboxEditModelIOS`.
class OmniboxControllerIOS : public AutocompleteController::Observer {
 public:
  OmniboxControllerIOS(OmniboxClient* client,
                       base::TimeDelta autocomplete_stop_timer_duration =
                           kAutocompleteDefaultStopTimerDuration);
  ~OmniboxControllerIOS() override;
  OmniboxControllerIOS(const OmniboxControllerIOS&) = delete;
  OmniboxControllerIOS& operator=(const OmniboxControllerIOS&) = delete;

  // The current_url field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // Cancels any pending asynchronous query. If `clear_result` is true, will
  // also erase the result set.
  void StopAutocomplete(bool clear_result) const;

  OmniboxClient* client() { return client_.get(); }

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }

  const AutocompleteController* autocomplete_controller() const {
    return autocomplete_controller_.get();
  }

  void SetAutocompleteControllerForTesting(
      std::unique_ptr<AutocompleteController> autocomplete_controller) {
    autocomplete_controller_ = std::move(autocomplete_controller);
  }

  base::WeakPtr<OmniboxControllerIOS> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<OmniboxClient> client_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  base::WeakPtrFactory<OmniboxControllerIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_
