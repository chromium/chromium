// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/ios_clipboard_context.h"

#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace data_controls {

IOSClipboardContext::IOSClipboardContext(const GURL& source_url,
                                         const GURL& destination_url,
                                         ProfileIOS* source_profile,
                                         ProfileIOS* destination_profile,
                                         ui::ClipboardMetadata metadata)
    : source_url_(source_url),
      destination_url_(destination_url),
      source_profile_(source_profile),
      destination_Profile_(destination_profile),
      metadata_(std::move(metadata)) {}

GURL IOSClipboardContext::source_url() const {
  return source_url_;
}

GURL IOSClipboardContext::destination_url() const {
  return destination_url_;
}

enterprise_connectors::ContentMetaData::CopiedTextSource
IOSClipboardContext::data_controls_copied_text_source() const {
  CHECK(destination_Profile_);
  using SourceType = enterprise_connectors::ContentMetaData::CopiedTextSource;

  SourceType copied_text_source;
  if (!source_profile_) {
    copied_text_source.set_context(SourceType::CLIPBOARD);
  } else if (source_profile_->IsOffTheRecord()) {
    copied_text_source.set_context(SourceType::INCOGNITO);
  } else if (source_profile_ == destination_Profile_) {
    copied_text_source.set_context(SourceType::SAME_PROFILE);
  } else {
    copied_text_source.set_context(SourceType::OTHER_PROFILE);
  }

  switch (copied_text_source.context()) {
    case SourceType::UNSPECIFIED:
    case SourceType::INCOGNITO:
      break;
    case SourceType::CLIPBOARD:
      // If the user does something like closing the browser between the time
      // they copy and then paste, the DTE might have a URL even though the lack
      // of browser context will make it impossible to know if the `SourceType`
      // is `SAME_PROFILE` or `OTHER_PROFILE`.
      //
      // In that case, we can be conservative and perform the same check as
      // `OTHER_PROFILE`. Note that this code path is unreachable in the case of
      // an incognito source URL as that is handled in the `set_context`
      // conditions above.
      [[fallthrough]];
    case SourceType::OTHER_PROFILE:
      // Only add a source URL if the other profile is getting the policy
      // applied at the machine scope, not the user scope.
      if (destination_Profile_->GetPrefs()->GetInteger(
              kDataControlsRulesScopePref) == policy::POLICY_SCOPE_USER) {
        break;
      }
      [[fallthrough]];
    case SourceType::SAME_PROFILE:
      if (source_url_.is_valid()) {
        copied_text_source.set_url(source_url_.spec());
      }
      break;
  }

  return copied_text_source;
}

ui::ClipboardFormatType IOSClipboardContext::format_type() const {
  return metadata_.format_type;
}

std::optional<size_t> IOSClipboardContext::size() const {
  return metadata_.size;
}

std::string IOSClipboardContext::source_active_user() const {
  // TODO(crbug.com/452926435): Refactor the ContentAreaUserProvider so it can
  // be used here to get the active_user.
  return std::string();
}

std::string IOSClipboardContext::destination_active_user() const {
  // TODO(crbug.com/452926435): Refactor the ContentAreaUserProvider so it can
  // be used here to get the active_user.
  return std::string();
}

}  // namespace data_controls
