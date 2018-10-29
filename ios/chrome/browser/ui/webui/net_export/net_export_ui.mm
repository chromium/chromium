// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/net_export/net_export_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/net_log/net_export_ui_constants.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/webui/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/show_mail_composer_context.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"
#include "net/log/net_log_capture_mode.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

web::WebUIIOSDataSource* CreateNetExportHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUINetExportHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  source->UseGzip();
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's member methods are expected to run on the UI thread.
class NetExportMessageHandler
    : public web::WebUIIOSMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler>,
      public net_log::NetExportFileWriter::StateObserver {
 public:
  NetExportMessageHandler();
  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages.
  void OnEnableNotifyUIWithState(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);

  // net_log::NetExportFileWriter::StateObserver implementation.
  void OnNewState(const base::DictionaryValue& state) override;

 private:
  // Send NetLog data via email.
  void SendEmail(const base::FilePath& file_to_send);

  void NotifyUIWithState(
      std::unique_ptr<base::DictionaryValue> file_writer_state);

  // Cache of GetApplicationContext()->GetNetExportFileWriter().
  // This is owned by the ApplicationContext.
  net_log::NetExportFileWriter* file_writer_;

  ScopedObserver<net_log::NetExportFileWriter,
                 net_log::NetExportFileWriter::StateObserver>
      state_observer_manager_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : file_writer_(GetApplicationContext()->GetNetExportFileWriter()),
      state_observer_manager_(this),
      weak_ptr_factory_(this) {
  file_writer_->Initialize();
}

NetExportMessageHandler::~NetExportMessageHandler() {
  file_writer_->StopNetLog(nullptr);
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  web_ui()->RegisterMessageCallback(
      net_log::kEnableNotifyUIWithStateHandler,
      base::BindRepeating(&NetExportMessageHandler::OnEnableNotifyUIWithState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStartNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnStartNetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStopNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnStopNetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kSendNetLogHandler,
      base::BindRepeating(&NetExportMessageHandler::OnSendNetLog,
                          base::Unretained(this)));
}

void NetExportMessageHandler::OnEnableNotifyUIWithState(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!state_observer_manager_.IsObservingSources()) {
    state_observer_manager_.Add(file_writer_);
  }
  NotifyUIWithState(file_writer_->GetState());
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  const base::Value::ListStorage& params = list->GetList();

  // Determine the capture mode.
  net::NetLogCaptureMode capture_mode = net::NetLogCaptureMode::Default();
  if (params.size() > 0 && params[0].is_string()) {
    capture_mode = net_log::NetExportFileWriter::CaptureModeFromString(
        params[0].GetString());
  }

  // Determine the max file size.
  uint64_t max_log_file_size = net_log::NetExportFileWriter::kNoLimit;
  // Unlike in desktop/Android net_export_ui, the size limit here is encoded
  // into a base::Value as a double; this is a behavior difference between
  // ValueResultFromWKResult and V8ValueConverter[Impl]::FromV8Value[Impl].
  if (params.size() > 1 && (params[1].is_int() || params[1].is_double()) &&
      params[1].GetDouble() > 0.0) {
    max_log_file_size = params[1].GetDouble();
  }

  file_writer_->StartNetLog(
      base::FilePath(), capture_mode, max_log_file_size,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      GetChannelString(), GetApplicationContext()->GetSystemNetworkContext());
}

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  file_writer_->StopNetLog(nullptr);
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::Bind(&NetExportMessageHandler::SendEmail, base::Unretained(this)));
}

void NetExportMessageHandler::OnNewState(const base::DictionaryValue& state) {
  NotifyUIWithState(state.CreateDeepCopy());
}

void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  if (file_to_send.empty())
    return;
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  NSString* subject = @"net_internals_log";
  NSString* body =
      @"Please add some informative text about the network issues.";
  int alert_title_id = IDS_IOS_NET_EXPORT_NO_EMAIL_ACCOUNTS_ALERT_TITLE;
  int alert_message_id = IDS_IOS_NET_EXPORT_NO_EMAIL_ACCOUNTS_ALERT_MESSAGE;

  ShowMailComposerContext* context =
      [[ShowMailComposerContext alloc] initWithToRecipients:nil
                                                    subject:subject
                                                       body:body
                             emailNotConfiguredAlertTitleId:alert_title_id
                           emailNotConfiguredAlertMessageId:alert_message_id];
  context.textFileToAttach = file_to_send;

  web::WebState* web_state = web_ui()->GetWebState();
  NetExportTabHelper::FromWebState(web_state)->ShowMailComposer(context);
}

void NetExportMessageHandler::NotifyUIWithState(
    std::unique_ptr<base::DictionaryValue> file_writer_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(web_ui());
  web_ui()->CallJavascriptFunction(net_log::kOnExportNetLogInfoChanged,
                                   *file_writer_state);
}

}  // namespace

NetExportUI::NetExportUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetExportMessageHandler>());
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateNetExportHTMLSource());
}
