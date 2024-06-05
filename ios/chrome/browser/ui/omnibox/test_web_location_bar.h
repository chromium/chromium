// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_WEB_LOCATION_BAR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_WEB_LOCATION_BAR_H_

#import "base/memory/raw_ptr.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "ios/chrome/browser/ui/omnibox/web_location_bar.h"

class TestWebLocationBar final : public WebLocationBar {
 public:
  TestWebLocationBar() = default;
  ~TestWebLocationBar() override;

  void SetWebState(web::WebState* web_state);
  void SetLocationBarModel(LocationBarModel* location_bar_model);

  web::WebState* GetWebState() override;
  void OnNavigate(const GURL& destination_url,
                  TemplateURLRef::PostContent* post_content,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool destination_url_entered_without_scheme,
                  const AutocompleteMatch& match) override {}
  LocationBarModel* GetLocationBarModel() override;

 private:
  raw_ptr<web::WebState> web_state_;
  raw_ptr<LocationBarModel> location_bar_model_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_TEST_WEB_LOCATION_BAR_H_
