// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_IMPL_H_

#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller.h"

@protocol LocationBarDelegate;
@protocol LocationBarURLLoader;

// A minimal implementation of WebOmniboxEditController. Designed to work with
// LocationBarMediator and LocationBarCoordinator.
// TODO(crbug.com/818641): downgrade from WebOmniboxEditController subclass
// straight to OmniboxEditController subclass once OmniboxViewIOS doesn't need
// it.
class WebOmniboxEditControllerImpl : public WebOmniboxEditController {
 public:
  WebOmniboxEditControllerImpl(id<LocationBarDelegate> delegate);
  ~WebOmniboxEditControllerImpl() override;

  void SetURLLoader(id<LocationBarURLLoader> URLLoader) {
    URLLoader_ = URLLoader;
  }

  // WebOmniboxEditController methods.
  web::WebState* GetWebState() override;
  void OnKillFocus() override;
  void OnSetFocus() override;

  // OmniboxEditController methods.
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  void OnChanged() override;
  LocationBarModel* GetLocationBarModel() override;
  const LocationBarModel* GetLocationBarModel() const override;

 private:
  __weak id<LocationBarDelegate> delegate_;
  __weak id<LocationBarURLLoader> URLLoader_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_CONTROLLER_IMPL_H_
