// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/data_controls_tab_helper.h"

#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/data_controls/clipboard_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/web/public/web_state.h"
#import "ui/base/clipboard/clipboard_metadata.h"
#import "url/gurl.h"

namespace data_controls {

DataControlsTabHelper::DataControlsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

DataControlsTabHelper::~DataControlsTabHelper() = default;

void DataControlsTabHelper::ShouldAllowCopy(
    base::OnceCallback<void(bool)> callback) {
  if (!base::FeatureList::IsEnabled(kEnableClipboardDataControlsIOS)) {
    std::move(callback).Run(true);
    return;
  }
  // TODO(crbug.com/444224082): Include size and format type for copy
  // operations.
  ui::ClipboardMetadata metadata;

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  const GURL& source_url = web_state_->GetLastCommittedURL();

  IsCopyAllowedByPolicy(source_url, metadata, profile, web_state_,
                        base::BindOnce(&DataControlsTabHelper::OnCopyAllowed,
                                       weak_factory_.GetWeakPtr(), source_url,
                                       std::move(callback)));
}

void DataControlsTabHelper::ShouldAllowPaste(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::ShouldAllowCut(
    base::OnceCallback<void(bool)> callback) {
  // "Cut" is treated the same way as "copy".
  ShouldAllowCopy(std::move(callback));
}

void DataControlsTabHelper::ShouldAllowShare(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

void DataControlsTabHelper::OnCopyAllowed(
    const GURL& source_url,
    base::OnceCallback<void(bool)> callback,
    CopyDecision decision) {
  // The user may have navigated away from the page from which the copy
  // operation was initiated. If the URL has changed, we should block the copy
  // operation as the original content is no longer available.
  if (source_url != web_state_->GetLastCommittedURL()) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/439549626): Store metadata when decision is
  // kAllow or kAllowAndProtect.
  bool allowed = (decision == CopyDecision::kAllow ||
                  decision == CopyDecision::kAllowAndProtect);
  std::move(callback).Run(allowed);
}

}  // namespace data_controls
