// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/tracing/tracing_ui.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/base_paths.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/ref_counted_memory.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/thread_restrictions.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/tracing/ios_tracing_controller.h"
#import "ios/chrome/grit/ios_resources.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"
#import "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#import "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace {

web::WebUIIOSDataSource* CreateTracingUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUITracingHost);

  source->SetDefaultResource(IDR_IOS_TRACING_HTML);
  return source;
}

base::FilePath GetTraceFilePath() {
  base::FilePath cache_dir;
  base::PathService::Get(base::DIR_CACHE, &cache_dir);
  return cache_dir.Append("manual_trace.perfetto-trace");
}

}  // namespace

TracingMessageHandler::TracingMessageHandler() = default;

TracingMessageHandler::~TracingMessageHandler() = default;

void TracingMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(&TracingMessageHandler::HandleStartTracing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopTracing",
      base::BindRepeating(&TracingMessageHandler::HandleStopTracing,
                          base::Unretained(this)));
}

void TracingMessageHandler::HandleStartTracing(const base::ListValue& args) {
  if (tracing_session_) {
    return;
  }

  perfetto::TraceConfig config =
      IOSTracingController::GetInstance().CreateDeveloperTraceConfig();

  tracing_session_ = perfetto::Tracing::NewTrace();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce([]() {
        base::FilePath trace_file = GetTraceFilePath();
        return base::File(trace_file, base::File::FLAG_CREATE_ALWAYS |
                                          base::File::FLAG_WRITE);
      }),
      base::BindOnce(
          [](base::WeakPtr<TracingMessageHandler> handler,
             perfetto::TraceConfig config, base::File file) {
            if (!file.IsValid()) {
              return;
            }
            if (!handler || !handler->tracing_session_) {
              return;
            }
            handler->tracing_session_->Setup(config, file.TakePlatformFile());
            handler->tracing_session_->Start();
            handler->web_ui()->CallJavascriptFunction("onRecordingStarted", {});
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(config)));
}

void TracingMessageHandler::HandleStopTracing(const base::ListValue& args) {
  if (!tracing_session_) {
    return;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(
          [](std::unique_ptr<perfetto::TracingSession> session) {
            session->StopBlocking();
          },
          std::move(tracing_session_)),
      base::BindOnce(&TracingMessageHandler::OnTracingStopped,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TracingMessageHandler::OnTracingStopped() {
  base::FilePath trace_file = GetTraceFilePath();

  // Use the native UIActivityViewController to share the file.
  // Note: this should be triggered from the active view controller, but since
  // we are in WebUI, we can find the key window's rootViewController.
  NSURL* file_url =
      [NSURL fileURLWithPath:base::apple::FilePathToNSString(trace_file)];
  UIActivityViewController* activity_vc =
      [[UIActivityViewController alloc] initWithActivityItems:@[ file_url ]
                                        applicationActivities:nil];
  UIWindow* key_window = GetAnyKeyWindow();
  UIViewController* top_controller = key_window.rootViewController;
  while (top_controller.presentedViewController) {
    top_controller = top_controller.presentedViewController;
  }

  if (top_controller) {
    activity_vc.popoverPresentationController.sourceView = top_controller.view;
    activity_vc.popoverPresentationController.sourceRect =
        CGRectMake(CGRectGetMidX(top_controller.view.bounds),
                   CGRectGetMidY(top_controller.view.bounds), 0, 0);
    activity_vc.popoverPresentationController.permittedArrowDirections = 0;

    base::WeakPtr<TracingMessageHandler> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    activity_vc.completionWithItemsHandler =
        ^(UIActivityType activityType, BOOL completed, NSArray* returnedItems,
          NSError* activityError) {
          if (weak_this) {
            weak_this->web_ui()->CallJavascriptFunction(
                "onFinishedProcessingTrace", {});
          }
        };

    [top_controller presentViewController:activity_vc
                                 animated:YES
                               completion:nil];
  } else {
    web_ui()->CallJavascriptFunction("onFinishedProcessingTrace", {});
  }
}

TracingUI::TracingUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<TracingMessageHandler>());
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateTracingUIHTMLSource());
}

TracingUI::~TracingUI() {}
