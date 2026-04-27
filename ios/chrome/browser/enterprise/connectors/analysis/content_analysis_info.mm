// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#import "base/rand_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#import "components/enterprise/connectors/core/content_area_user_provider.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state.h"

namespace enterprise_connectors {

ContentAnalysisInfo::ContentAnalysisInfo(const GURL& url,
                                         AnalysisSettings settings,
                                         ContentAnalysisRequest::Reason reason,
                                         const web::WebState& web_state)
    : settings_(std::move(settings)), url_(url) {
  tab_url_ = web_state.GetLastCommittedURL();
  title_ = base::UTF16ToUTF8(web_state.GetTitle());
  reason_ = reason;
  user_action_id_ = base::HexEncode(base::RandBytesAsVector(128));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state.GetBrowserState()));

  if (identity_manager) {
    identity_manager_ = identity_manager->GetWeakPtr();
  }
}

ContentAnalysisInfo::~ContentAnalysisInfo() = default;

void ContentAnalysisInfo::InitializeRequest(
    BinaryUploadRequest* request,
    bool include_enterprise_only_fields) {
  InitializeBinaryUploadRequest(request, *this, include_enterprise_only_fields);
}

const GURL& ContentAnalysisInfo::url() const {
  return url_;
}

const GURL& ContentAnalysisInfo::tab_url() const {
  return tab_url_;
}

signin::IdentityManager* ContentAnalysisInfo::identity_manager() const {
  return identity_manager_.get();
}

const AnalysisSettings& ContentAnalysisInfo::settings() const {
  return settings_;
}

int ContentAnalysisInfo::user_action_requests_count() const {
  // iOS can only download 1 file at a time so action count is always 1.
  return 1;
}

std::string ContentAnalysisInfo::tab_title() const {
  return title_;
}

std::string ContentAnalysisInfo::user_action_id() const {
  return user_action_id_;
}

std::string ContentAnalysisInfo::email() const {
  if (!identity_manager()) {
    return std::string();
  }
  return identity_manager()
      ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

ContentAnalysisRequest::Reason ContentAnalysisInfo::reason() const {
  return reason_;
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
ContentAnalysisInfo::referrer_chain() const {
  return {};
}

google::protobuf::RepeatedPtrField<std::string>
ContentAnalysisInfo::frame_url_chain() const {
  return {};
}

std::string ContentAnalysisInfo::GetContentAreaAccountEmail() const {
  return GetActiveContentAreaUser(identity_manager(), tab_url());
}

}  // namespace enterprise_connectors
