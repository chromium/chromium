// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state.h"

namespace enterprise_connectors {

ContentAnalysisInfo::ContentAnalysisInfo(const GURL& url,
                                         AnalysisSettings settings,
                                         web::WebState* web_state)
    : settings_(std::move(settings)),
      url_(url),
      tab_url_(web_state->GetLastCommittedURL()) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));

  if (identity_manager) {
    identity_manager_ = identity_manager->GetWeakPtr();
  }
}

ContentAnalysisInfo::~ContentAnalysisInfo() = default;

const GURL& ContentAnalysisInfo::url() const {
  return url_;
}

const GURL& ContentAnalysisInfo::tab_url() const {
  return tab_url_;
}

signin::IdentityManager* ContentAnalysisInfo::identity_manager() const {
  return identity_manager_.get();
}

const enterprise_connectors::AnalysisSettings& ContentAnalysisInfo::settings()
    const {
  return settings_;
}

int ContentAnalysisInfo::user_action_requests_count() const {
  NOTREACHED();
}

std::string ContentAnalysisInfo::tab_title() const {
  NOTREACHED();
}

std::string ContentAnalysisInfo::user_action_id() const {
  NOTREACHED();
}

std::string ContentAnalysisInfo::email() const {
  NOTREACHED();
}

enterprise_connectors::ContentAnalysisRequest::Reason
ContentAnalysisInfo::reason() const {
  NOTREACHED();
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
ContentAnalysisInfo::referrer_chain() const {
  return {};
}

google::protobuf::RepeatedPtrField<std::string>
ContentAnalysisInfo::frame_url_chain() const {
  return {};
}

}  // namespace enterprise_connectors
