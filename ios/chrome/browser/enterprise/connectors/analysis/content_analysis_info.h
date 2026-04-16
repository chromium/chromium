// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_

#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#import "components/enterprise/connectors/core/content_analysis_info_base.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/web/public/download/download_task.h"

namespace enterprise_connectors {

// IOS implementation of `ContentAnalysisInfoBase`. It provides analysis
// settings and content analysis context for a given content analysis action.
class ContentAnalysisInfo : public ContentAnalysisInfoBase {
 public:
  explicit ContentAnalysisInfo(
      // The URL containing the contents for the analysis request.
      const GURL& url,
      AnalysisSettings settings,
      ContentAnalysisRequest::Reason reason,
      base::WeakPtr<web::WebState> web_state);
  ~ContentAnalysisInfo() override;

  // ContentAnalysisInfoBase override:
  void InitializeRequest(BinaryUploadRequest* request,
                         bool include_enterprise_only_fields) override;
  const GURL& url() const override;
  const GURL& tab_url() const override;
  signin::IdentityManager* identity_manager() const override;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override;
  const AnalysisSettings& settings() const override;
  std::string GetContentAreaAccountEmail() const override;
  int user_action_requests_count() const override;
  std::string tab_title() const override;
  std::string user_action_id() const override;
  std::string email() const override;
  ContentAnalysisRequest::Reason reason() const override;

 private:
  // The content analysis settings.
  AnalysisSettings settings_;

  // The URL containing the contents for the analysis request.
  GURL url_;
  GURL tab_url_;
  // The title corresponding to the WebState triggering the scan.
  std::string title_ = std::string();
  // The reason that triggers the scan.
  ContentAnalysisRequest::Reason reason_ = ContentAnalysisRequest::UNKNOWN;
  // The unique id for identifying the user action.
  std::string user_action_id_;
  base::WeakPtr<signin::IdentityManager> identity_manager_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
