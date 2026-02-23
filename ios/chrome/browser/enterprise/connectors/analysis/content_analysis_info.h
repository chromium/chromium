// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_

#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/content_analysis_info_base.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/web/public/download/download_task.h"

namespace enterprise_connectors {

// IOS implementation of `ContentAnalysisInfoBase`. It provides analysis
// settings and content analysis context for a given content analysis action.
//
// TODO(crbug.com/485126116): Update this class to include more functionalities
// needed for the content analysis action.
class ContentAnalysisInfo : public ContentAnalysisInfoBase {
 public:
  explicit ContentAnalysisInfo(
      // The URL containing the contents for the analysis request.
      const GURL& url,
      AnalysisSettings settings,
      web::WebState* web_state);
  ~ContentAnalysisInfo();

  // ContentAnalysisInfoBase override:
  const GURL& url() const override;
  const GURL& tab_url() const override;
  signin::IdentityManager* identity_manager() const override;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override;
  const AnalysisSettings& settings() const override;

 private:
  int user_action_requests_count() const override;
  std::string tab_title() const override;
  std::string user_action_id() const override;
  std::string email() const override;
  ContentAnalysisRequest::Reason reason() const override;

  // The content analysis settings.
  AnalysisSettings settings_;

  // The URL containing the contents for the analysis request.
  GURL url_;
  GURL tab_url_;
  base::WeakPtr<signin::IdentityManager> identity_manager_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
