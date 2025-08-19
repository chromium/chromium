// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/data_controls_tab_helper.h"

#import "base/functional/callback.h"
#import "ios/web/public/web_state.h"

namespace data_controls {

DataControlsTabHelper::DataControlsTabHelper(web::WebState* web_state) {}

DataControlsTabHelper::~DataControlsTabHelper() = default;

void DataControlsTabHelper::ShouldAllowCopy(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::ShouldAllowPaste(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::ShouldAllowCut(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::ShouldAllowShare(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

}  // namespace data_controls
