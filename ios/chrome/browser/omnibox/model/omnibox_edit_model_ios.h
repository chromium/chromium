// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_

#import <stddef.h>

#import <map>
#import <memory>
#import <string>
#import <string_view>

#import "base/compiler_specific.h"
#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox.mojom-shared.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "ui/base/window_open_disposition.h"
#import "url/gurl.h"

@class OmniboxAutocompleteController;
class OmniboxClient;
class OmniboxControllerIOS;
@class OmniboxMetricsRecorder;
@class OmniboxTextController;

class OmniboxEditModelIOS {
 public:
  OmniboxEditModelIOS(OmniboxControllerIOS* controller,
                      OmniboxClient* client,
                      OmniboxTextModel* text_model,
                      OmniboxMetricsRecorder* metrics_recorder);
  virtual ~OmniboxEditModelIOS();
  OmniboxEditModelIOS(const OmniboxEditModelIOS&) = delete;
  OmniboxEditModelIOS& operator=(const OmniboxEditModelIOS&) = delete;

  void set_omnibox_autocomplete_controller(
      OmniboxAutocompleteController* omnibox_autocomplete_controller) {
    omnibox_autocomplete_controller_ = omnibox_autocomplete_controller;
  }

  // Asks the browser to load `match` or execute one of its actions
  // according to `selection`.
  //
  // OpenMatch() needs to know the original text that drove this action.  If
  // `pasted_text` is non-empty, this is a Paste-And-Go/Search action, and
  // that's the relevant input text.  Otherwise, the relevant input text is
  // either the user text or the display URL, depending on if user input is
  // in progress.
  //
  // `match` is passed by value for two reasons:
  // (1) This function needs to modify `match`, so a const ref isn't
  //     appropriate.  Callers don't actually care about the modifications, so a
  //     pointer isn't required.
  // (2) The passed-in match is, on the caller side, typically coming from data
  //     associated with the popup.  Since this call can close the popup, that
  //     could clear that data, leaving us with a pointer-to-garbage.  So at
  //     some point someone needs to make a copy of the match anyway, to
  //     preserve it past the popup closure.
  void OpenMatch(OmniboxPopupSelection selection,
                 AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const std::u16string& pasted_text,
                 base::TimeTicks match_selection_timestamp = base::TimeTicks());

  void set_text_controller(OmniboxTextController* text_controller);

  // This calls `OpenMatch` directly for the few remaining `OmniboxEditModelIOS`
  // test cases that require explicit control over match content. For new
  // tests, and for non-test code, use `OpenSelection`.
  void OpenMatchForTesting(
      AutocompleteMatch match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t index,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  base::WeakPtr<OmniboxEditModelIOS> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
 private:
  friend class OmniboxControllerIOSTest;
  friend class TestOmniboxEditModelIOS;

  AutocompleteController* autocomplete_controller() const;

  // Owns this.
  raw_ptr<OmniboxControllerIOS> controller_;

  // The omnibox client.
  raw_ptr<OmniboxClient> client_;

  // The omnibox text model containing the text state.
  raw_ptr<OmniboxTextModel> text_model_;

  // The text controller.
  __weak OmniboxTextController* text_controller_ = nil;

  // The autocomplete controller.
  __weak OmniboxAutocompleteController* omnibox_autocomplete_controller_ = nil;

  // The metrics recorder
  __weak OmniboxMetricsRecorder* metrics_recorder_ = nil;

  base::WeakPtrFactory<OmniboxEditModelIOS> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_
