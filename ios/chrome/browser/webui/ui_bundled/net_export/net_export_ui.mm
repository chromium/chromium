// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/net_export/net_export_ui.h"

#import <memory>
#import <string>

#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/string_util.h"
#import "base/values.h"
#import "components/grit/dev_ui_components_resources.h"
#import "components/net_log/net_export_file_writer.h"
#import "components/net_log/net_export_ui_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/model/show_mail_composer_context.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"
#import "net/log/net_log_capture_mode.h"
#import "net/url_request/url_request_context_getter.h"

namespace {

web::WebUIIOSDataSource* CreateNetExportHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUINetExportHost);

  source->UseStringsJs();
  source->AddResourcePath(net_log::kNetExportUICSS, IDR_NET_LOG_NET_EXPORT_CSS);
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's member methods are expected to run on the UI thread.
class NetExportMessageHandler
    : public web::WebUIIOSMessageHandler,
      public net_log::NetExportFileWriter::StateObserver {
 public:
  NetExportMessageHandler();

  NetExportMessageHandler(const NetExportMessageHandler&) = delete;
  NetExportMessageHandler& operator=(const NetExportMessageHandler&) = delete;

  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages.
  void OnEnableNotifyUIWithState(const base::Value::List& list);
  void OnStartNetLog(const base::Value::List& list);
  void OnStopNetLog(const base::Value::List& list);
  void OnSendNetLog(const base::Value::List& list);

  // net_log::NetExportFileWriter::StateObserver implementation.
  void OnNewState(const base::Value::Dict& state) override;

 private:
  // Send NetLog data via email.
  void SendEmail(const base::FilePath& file_to_send);

  void NotifyUIWithState(
      const base::Value::Dict& file_writer_state);

  // Cache of GetApplicationContext()->GetNetExportFileWriter().
  // This is owned by the ApplicationContext.
  raw_ptr<net_log::NetExportFileWriter> file_writer_;

  base::ScopedObservation<net_log::NetExportFileWriter,
                          net_log::NetExportFileWriter::StateObserver>
      state_observation_manager_{this};

  base::WeakPtrFactory<NetExportMessageHandler> weak_factory_{this};
};

NetExportMessageHandler::NetExportMessageHandler()
    : file_writer_(GetApplicationContext()->GetNetExportFileWriter()) {
  file_writer_->Initialize();
}

NetExportMessageHandler::~NetExportMessageHandler() {
  file_writer_->StopNetLog();
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
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!state_observation_manager_.IsObserving()) {
    state_observation_manager_.Observe(file_writer_.get());
  }
  NotifyUIWithState(file_writer_->GetState());
}

void NetExportMessageHandler::OnStartNetLog(const base::Value::List& params) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // Determine the capture mode.
  net::NetLogCaptureMode capture_mode = net::NetLogCaptureMode::kDefault;
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

void NetExportMessageHandler::OnStopNetLog(const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  file_writer_->StopNetLog();
}

void NetExportMessageHandler::OnSendNetLog(const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  file_writer_->GetFilePathToCompletedLog(base::BindOnce(
      &NetExportMessageHandler::SendEmail, weak_factory_.GetWeakPtr()));
}

void NetExportMessageHandler::OnNewState(const base::Value::Dict& state) {
  NotifyUIWithState(state);
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
  NetExportTabHelper::GetOrCreateForWebState(web_state)->ShowMailComposer(
      context);
}

void NetExportMessageHandler::NotifyUIWithState(
    const base::Value::Dict& file_writer_state) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(web_ui());

  base::Value event(net_log::kNetLogInfoChangedEvent);
  base::ValueView args[] = {event, file_writer_state};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

}  // namespace

NetExportUI::NetExportUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<NetExportMessageHandler>());
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateNetExportHTMLSource());
}
