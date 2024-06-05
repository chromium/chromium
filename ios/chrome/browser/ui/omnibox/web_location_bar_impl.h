// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_IMPL_H_

#include "ios/chrome/browser/ui/omnibox/web_location_bar.h"

@protocol LocationBarURLLoader;
@protocol OmniboxControllerDelegate;
@protocol OmniboxFocusDelegate;

// A minimal implementation of WebLocationBar. Designed to work
// with LocationBarMediator and LocationBarCoordinator.
// TODO(crbug.com/40565667): downgrade from WebLocationBar subclass straight to
// WebLocationBar once OmniboxViewIOS doesn't need it.
class WebLocationBarImpl : public WebLocationBar {
 public:
  explicit WebLocationBarImpl(id<OmniboxControllerDelegate> delegate);
  ~WebLocationBarImpl() override;

  void SetURLLoader(id<LocationBarURLLoader> URLLoader) {
    URLLoader_ = URLLoader;
  }

  // WebLocationBar methods.
  web::WebState* GetWebState() override;
  void OnNavigate(const GURL& destination_url,
                  TemplateURLRef::PostContent* post_content,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool destination_url_entered_without_scheme,
                  const AutocompleteMatch& match) override;
  LocationBarModel* GetLocationBarModel() override;

 private:
  __weak id<OmniboxControllerDelegate> delegate_;
  __weak id<LocationBarURLLoader> URLLoader_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_IMPL_H_
