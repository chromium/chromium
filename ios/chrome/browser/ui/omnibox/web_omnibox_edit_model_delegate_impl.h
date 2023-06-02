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
// TODO(crbug.com/1404748): Adjust the previous TODO as OmniboxEditModelDelegate
//  no longer exists.
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
  void OnNavigate(const GURL& destination_url,
                  TemplateURLRef::PostContent* post_content,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool destination_url_entered_without_scheme,
                  const AutocompleteMatch& match) override;
  LocationBarModel* GetLocationBarModel() override;

 private:
  __weak id<OmniboxControllerDelegate> delegate_;
  __weak id<OmniboxFocusDelegate> focus_delegate_;
  __weak id<LocationBarURLLoader> URLLoader_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_OMNIBOX_EDIT_MODEL_DELEGATE_IMPL_H_
