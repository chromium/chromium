// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/pasteboard_content_handler_ios.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"
#import "components/policy/core/browser/browser_policy_connector.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace enterprise_connectors {

PasteboardContentHandlerIOS::PasteboardContentHandlerIOS(
    PasteboardInfo pasteboard_info,
    BinaryUploadService* upload_service,
    ReportingEventRouter* router,
    ContentMetaData::CopiedTextSource copied_source,
    std::unique_ptr<ContentAnalysisInfo> content_analysis_info,
    base::RepeatingCallback<policy::BrowserPolicyConnector*()> policy_callback,
    base::OnceCallback<void(RequestHandlerResult)> result_callback)
    : pasteboard_info_(std::move(pasteboard_info)),
      upload_service_(upload_service),
      router_(router),
      copied_source_(copied_source),
      content_analysis_info_(std::move(content_analysis_info)),
      policy_callback_(std::move(policy_callback)),
      result_callback_(std::move(result_callback)) {
  CHECK(content_analysis_info_);
}

PasteboardContentHandlerIOS::~PasteboardContentHandlerIOS() = default;

void PasteboardContentHandlerIOS::StartContentAnalysisRequest() {
  // This should not be called when there is an ongoing scan request.
  CHECK(!text_request_handler_);
  CHECK(!image_request_handler_);

  // This will only call OnGetRequestHandlerResults when both text request
  // and image request have received their respective scan result.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2U,
      base::BindOnce(&PasteboardContentHandlerIOS::OnGetRequestHandlerResults,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!pasteboard_info_.text.empty()) {
    text_request_handler_ = ClipboardRequestHandler::Create(
        content_analysis_info_.get(), upload_service_, router_,
        pasteboard_info_.destination_url, ClipboardRequestHandler::Type::kText,
        DeepScanAccessPoint::PASTE, copied_source_,
        content_analysis_info_->GetContentAreaAccountEmail(),
        /*content_transfer_method=*/"", std::move(pasteboard_info_.text),
        base::BindOnce(&PasteboardContentHandlerIOS::OnGetTextResult,
                       weak_ptr_factory_.GetWeakPtr())
            .Then(barrier_closure),
        policy_callback_);
    text_request_handler_->UploadData();
  } else {
    text_result_.final_result = FinalContentAnalysisResult::SUCCESS;
    text_result_action_ = RequestHandlerResultActionLevel::kNotScan;
    barrier_closure.Run();
  }

  if (!pasteboard_info_.image.empty()) {
    image_request_handler_ = ClipboardRequestHandler::Create(
        content_analysis_info_.get(), upload_service_, router_,
        pasteboard_info_.destination_url, ClipboardRequestHandler::Type::kImage,
        DeepScanAccessPoint::PASTE, copied_source_,
        content_analysis_info_->GetContentAreaAccountEmail(),
        /*content_transfer_method=*/"", std::move(pasteboard_info_.image),
        base::BindOnce(&PasteboardContentHandlerIOS::OnGetImageResult,
                       weak_ptr_factory_.GetWeakPtr())
            .Then(barrier_closure),
        policy_callback_);
    image_request_handler_->UploadData();
  } else {
    image_result_.final_result = FinalContentAnalysisResult::SUCCESS;
    image_result_action_ = RequestHandlerResultActionLevel::kNotScan;
    barrier_closure.Run();
  }
}

void PasteboardContentHandlerIOS::OnGetTextResult(RequestHandlerResult result) {
  text_result_ = std::move(result);
  text_result_action_ = ResultToActionLevel(text_result_);
}

void PasteboardContentHandlerIOS::OnGetImageResult(
    RequestHandlerResult result) {
  image_result_ = std::move(result);
  image_result_action_ = ResultToActionLevel(image_result_);
}

void PasteboardContentHandlerIOS::OnGetRequestHandlerResults() {
  // Provide the highest action result (kBlock > kWarn > kAudit).
  if (text_result_action_ > image_result_action_) {
    std::move(result_callback_).Run(std::move(text_result_));
  } else if (image_result_action_ > text_result_action_) {
    std::move(result_callback_).Run(std::move(image_result_));
  } else {
    // Default to `text_result_` when both of image and text strings are empty.
    std::move(result_callback_).Run(std::move(text_result_));
  }
}

void PasteboardContentHandlerIOS::ReportWarningBypass() {
  if (text_result_action_ == RequestHandlerResultActionLevel::kWarn) {
    text_request_handler_->ReportWarningBypass(
        /*user_justification=*/std::nullopt);
  }

  if (image_result_action_ == RequestHandlerResultActionLevel::kWarn) {
    image_request_handler_->ReportWarningBypass(
        /*user_justification=*/std::nullopt);
  }
}

}  // namespace enterprise_connectors
