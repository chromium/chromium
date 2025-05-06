// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_

#import <memory>

#import "base/compiler_specific.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/search_engines/template_url_starter_pack_data.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

class OmniboxClient;
class OmniboxViewBase;

// This class controls the various services that can modify the content of the
// omnibox, including `AutocompleteController` and `OmniboxEditModelIOS`.
class OmniboxControllerIOS : public AutocompleteController::Observer {
 public:
  OmniboxControllerIOS(OmniboxViewBase* view,
                       std::unique_ptr<OmniboxClient> client,
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

  // Starts an autocomplete prefetch request so that zero-prefix providers can
  // optionally start a prefetch request to warm up the their underlying
  // service(s) and/or optionally cache their otherwise async response.
  // Virtual for testing.
  virtual void StartZeroSuggestPrefetch();

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  OmniboxClient* client() { return client_.get(); }

  OmniboxEditModelIOS* edit_model() { return edit_model_.get(); }

  void SetEditModelForTesting(std::unique_ptr<OmniboxEditModelIOS> edit_model) {
    edit_model_ = std::move(edit_model);
  }

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

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  // Returns the header string associated with `suggestion_group_id`, or an
  // empty string if `suggestion_group_id` is not found in the results.
  std::u16string GetHeaderForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Returns whether or not the row for a particular match should be hidden in
  // the UI. This is currently used to hide suggestions in the 'Gemini' scope
  // when the starter pack expansion feature is enabled.
  bool IsSuggestionHidden(const AutocompleteMatch& match) const;

  // Returns whether or not `suggestion_group_id` should be collapsed in the UI.
  // This method takes into account both the user's stored prefs as well as
  // the server-provided visibility hint. Returns false if `suggestion_group_id`
  // is not found in the results.
  bool IsSuggestionGroupHidden(omnibox::GroupId suggestion_group_id) const;

  // Sets the UI collapsed/expanded state of the `suggestion_group_id` in the
  // user's stored prefs based on the value of `hidden`. Does nothing if
  // `suggestion_group_id` is not found in the results.
  void SetSuggestionGroupHidden(omnibox::GroupId suggestion_group_id,
                                bool hidden) const;

 private:
  // Stores the bitmap, using `icon_url` as the key in
  // `edit_model_->icon_bitmaps_` if provided, or `result_index` in
  // `edit_model_->rich_suggestion_bitmaps_` otherwise.
  void SetRichSuggestionBitmap(int result_index,
                               const GURL& icon_url,
                               const SkBitmap& bitmap);

  // Called when the prefs for the visibility of groups changes.
  void OnSuggestionGroupVisibilityPrefChanged();

  std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // `edit_model_` may indirectly contains raw pointers (e.g.
  // `edit_model_->current_match_->provider`) into `AutocompleteProvider`
  // objects owned by `autocomplete_controller_`.  Because of this (per
  // docs/dangling_ptr_guide.md) the `edit_model_` field needs to be declared
  // *after* the `autocomplete_controller_` field.
  std::unique_ptr<OmniboxEditModelIOS> edit_model_;

  // Observes changes to the prefs for the visibility of groups.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxControllerIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_CONTROLLER_IOS_H_
