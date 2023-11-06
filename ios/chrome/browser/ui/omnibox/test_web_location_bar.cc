// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/test_web_location_bar.h"

#include "components/omnibox/browser/location_bar_model.h"
#include "url/gurl.h"

TestWebLocationBar::~TestWebLocationBar() {
  web_state_ = nullptr;
  location_bar_model_ = nullptr;
}

web::WebState* TestWebLocationBar::GetWebState() {
  return web_state_;
}
void TestWebLocationBar::SetWebState(web::WebState* web_state) {
  web_state_ = web_state;
}

LocationBarModel* TestWebLocationBar::GetLocationBarModel() {
  return location_bar_model_;
}

void TestWebLocationBar::SetLocationBarModel(
    LocationBarModel* location_bar_model) {
  location_bar_model_ = location_bar_model;
}
