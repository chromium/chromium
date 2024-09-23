// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_H_

#include "components/search_engines/template_url.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class LocationBarModel;
struct AutocompleteMatch;

namespace web {
class WebState;
}

// An interface providing information about the current page, the navigation
// entry, and the omnibox edit. Similar to //c/b/ui/location_bar/location_bar.h.
class WebLocationBar {
 public:
  WebLocationBar(const WebLocationBar&) = delete;
  WebLocationBar& operator=(const WebLocationBar&) = delete;

  // Returns the WebState of the currently active tab.
  virtual web::WebState* GetWebState() = 0;

  virtual void OnNavigate(const GURL& destination_url,
                          TemplateURLRef::PostContent* post_content,
                          WindowOpenDisposition disposition,
                          ui::PageTransition transition,
                          bool destination_url_entered_without_scheme,
                          const AutocompleteMatch& match) = 0;

  virtual LocationBarModel* GetLocationBarModel() = 0;

 protected:
  WebLocationBar();
  virtual ~WebLocationBar();
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_WEB_LOCATION_BAR_H_
