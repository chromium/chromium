// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTEBOARD_CONTENT_HANDLER_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTEBOARD_CONTENT_HANDLER_IOS_H_

#import <memory>
#import <string>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/common.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "url/gurl.h"

namespace policy {
class BrowserPolicyConnector;
}

namespace enterprise_connectors {

class BinaryUploadService;
class ClipboardRequestHandler;
class ContentAnalysisInfo;
class ReportingEventRouter;

struct PasteboardInfo {
  // The concatenated string of text items in the pasteboard.
  std::string text;

  // The Base64 encoded string of the first image item in the pasteboard.
  std::string image;

  // The gurl of the webpage that the user is trying to paste into.
  GURL destination_url;
};

// A handler that uploads the text string and image string in the
// `PasteboardInfo` to WebProtect for scanning. Then provide the highest
// action level `RequestHandlerResult` to `result_callback`.
class PasteboardContentHandlerIOS {
 public:
  // `pasteboard_info`: Contains a text string, image string and a GURL of paste
  //                   destination.
  // `upload_service`: The service we use for upload the content for scanning.
  // `router`: The router for reporting warning bypass.
  // `copied_source`: Source information like GURL or profile of the copied
  //                 content.
  // `content_analysis_info` : Data or info like `AnalysisSettings` for the
  //                          scanning request.
  // `policy_callback`: Callback to provide platform specific
  //                   BrowserPolicyConnector for the request handler.
  // `result_callback`: Callback to provide the highest action level
  //                   `RequestHandlerResult` (Block > Warn > Audit).
  PasteboardContentHandlerIOS(
      PasteboardInfo pasteboard_info,
      BinaryUploadService* upload_service,
      ReportingEventRouter* router,
      ContentMetaData::CopiedTextSource copied_source,
      std::unique_ptr<ContentAnalysisInfo> content_analysis_info,
      base::RepeatingCallback<policy::BrowserPolicyConnector*()>
          policy_callback,
      base::OnceCallback<void(RequestHandlerResult)> result_callback);

  PasteboardContentHandlerIOS(const PasteboardContentHandlerIOS&) = delete;
  PasteboardContentHandlerIOS& operator=(const PasteboardContentHandlerIOS&) =
      delete;

  ~PasteboardContentHandlerIOS();

  // Start two separate Content Analysis Requests for text and image if they
  // exist.
  void StartContentAnalysisRequest();

  // Report warning bypass using the `text_request_handler_` or
  // `image_request_handler_` if their respective scan results is/are WARN and
  // user chooses to bypass.
  void ReportWarningBypass();

 private:
  void OnGetTextResult(RequestHandlerResult result);
  void OnGetImageResult(RequestHandlerResult result);
  void OnGetRequestHandlerResults();

  PasteboardInfo pasteboard_info_;

  // Safe to use raw_ptr as these are keyed services and tied to ProfileIOS,
  // PasteboardContentHandlerIOS should be created in the DataControlsTabHelper
  // and should not outlive the ProfileIOS.
  raw_ptr<BinaryUploadService> upload_service_;
  raw_ptr<ReportingEventRouter> router_;

  ContentMetaData::CopiedTextSource copied_source_;
  std::unique_ptr<ContentAnalysisInfo> content_analysis_info_;
  base::RepeatingCallback<policy::BrowserPolicyConnector*()> policy_callback_;
  std::unique_ptr<ClipboardRequestHandler> text_request_handler_;
  std::unique_ptr<ClipboardRequestHandler> image_request_handler_;

  RequestHandlerResult text_result_;
  RequestHandlerResult image_result_;
  RequestHandlerResultActionLevel text_result_action_ =
      RequestHandlerResultActionLevel::kNotScan;
  RequestHandlerResultActionLevel image_result_action_ =
      RequestHandlerResultActionLevel::kNotScan;

  // A callback to invoke with the highest action level from either text or
  // image scan result. (Block > Warn > Audit)
  base::OnceCallback<void(RequestHandlerResult)> result_callback_;

  base::WeakPtrFactory<PasteboardContentHandlerIOS> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTEBOARD_CONTENT_HANDLER_IOS_H_
