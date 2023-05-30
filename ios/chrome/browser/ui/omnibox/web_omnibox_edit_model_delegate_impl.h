// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_IMPL_H_

#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_model_delegate.h"

@protocol LocationBarURLLoader;
@protocol OmniboxControllerDelegate;
@protocol OmniboxFocusDelegate;

// A minimal implementation of WebOmniboxEditModelDelegate. Designed to work
// with LocationBarMediator and LocationBarCoordinator.
// TODO(crbug.com/818641): downgrade from WebOmniboxEditModelDelegate subclass
// straight to OmniboxEditModelDelegate subclass once OmniboxViewIOS doesn't
// need it.
class WebOmniboxEditModelDelegateImpl : public WebOmniboxEditModelDelegate {
 public:
  WebOmniboxEditModelDelegateImpl(id<OmniboxControllerDelegate> delegate,
                                  id<OmniboxFocusDelegate> focus_delegate);
  ~WebOmniboxEditModelDelegateImpl() override;

  void SetURLLoader(id<LocationBarURLLoader> URLLoader) {
    URLLoader_ = URLLoader;
  }

  // WebOmniboxEditModelDelegate methods.
  web::WebState* GetWebState() override;
  void OnKillFocus() override;
  void OnSetFocus() override;

  // OmniboxEditModelDelegate methods.
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  void OnInputInProgress(bool in_progress) override {}
  void OnChanged() override;
  void OnPopupVisibilityChanged() override {}
  LocationBarModel* GetLocationBarModel() override;
  const LocationBarModel* GetLocationBarModel() const override;

 private:
  __weak id<OmniboxControllerDelegate> delegate_;
  __weak id<OmniboxFocusDelegate> focus_delegate_;
  __weak id<LocationBarURLLoader> URLLoader_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_IMPL_H_
