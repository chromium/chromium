// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals_message_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

namespace content {

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler()
    : WebRTCInternalsMessageHandler(WebRTCInternals::GetInstance()) {}

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler(
    WebRTCInternals* webrtc_internals)
    : webrtc_internals_(webrtc_internals) {
  DCHECK(webrtc_internals);
  webrtc_internals_->AddObserver(this);
}

WebRTCInternalsMessageHandler::~WebRTCInternalsMessageHandler() {
  webrtc_internals_->RemoveObserver(this);
}

void WebRTCInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getStandardStats",
      base::BindRepeating(&WebRTCInternalsMessageHandler::OnGetStandardStats,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "enableAudioDebugRecordings",
      base::BindRepeating(
          &WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled,
          base::Unretained(this), true));

  web_ui()->RegisterMessageCallback(
      "disableAudioDebugRecordings",
      base::BindRepeating(
          &WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled,
          base::Unretained(this), false));

  web_ui()->RegisterMessageCallback(
      "enableEventLogRecordings",
      base::BindRepeating(
          &WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled,
          base::Unretained(this), true));

  web_ui()->RegisterMessageCallback(
      "disableEventLogRecordings",
      base::BindRepeating(
          &WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled,
          base::Unretained(this), false));

  web_ui()->RegisterMessageCallback(
      "finishedDOMLoad",
      base::BindRepeating(&WebRTCInternalsMessageHandler::OnDOMLoadDone,
                          base::Unretained(this)));
}

RenderFrameHost* WebRTCInternalsMessageHandler::GetWebRTCInternalsHost() {
  RenderFrameHost* host = web_ui()->GetWebContents()->GetPrimaryMainFrame();
  if (host) {
    // Make sure we only ever execute the script in the webrtc-internals page.
    const GURL url(host->GetLastCommittedURL());
    if (!url.SchemeIs(kChromeUIScheme) ||
        url.host() != kChromeUIWebRTCInternalsHost) {
      // Some other page is currently loaded even though we might be in the
      // process of loading webrtc-internals.  So, the current RFH is not the
      // one we're waiting for.
      host = nullptr;
    }
  }

  return host;
}

void WebRTCInternalsMessageHandler::OnGetStandardStats(
    const base::Value::List& /* unused_list */) {
  for (auto* host : PeerConnectionTrackerHost::GetAllHosts()) {
    host->GetStandardStats();
  }
}

void WebRTCInternalsMessageHandler::OnSetAudioDebugRecordingsEnabled(
    bool enable,
    const base::Value::List& /* unused_list */) {
  if (enable) {
    webrtc_internals_->EnableAudioDebugRecordings(web_ui()->GetWebContents());
  } else {
    webrtc_internals_->DisableAudioDebugRecordings();
  }
}

void WebRTCInternalsMessageHandler::OnSetEventLogRecordingsEnabled(
    bool enable,
    const base::Value::List& /* unused_list */) {
  if (!webrtc_internals_->CanToggleEventLogRecordings()) {
    LOG(WARNING) << "Cannot toggle WebRTC event logging.";
    return;
  }

  if (enable) {
    webrtc_internals_->EnableLocalEventLogRecordings(
        web_ui()->GetWebContents());
  } else {
    webrtc_internals_->DisableLocalEventLogRecordings();
  }
}

void WebRTCInternalsMessageHandler::OnDOMLoadDone(
    const base::Value::List& args_list) {
  CHECK_GE(args_list.size(), 1u);

  const std::string callback_id = args_list[0].GetString();
  AllowJavascript();

  webrtc_internals_->UpdateObserver(this);

  base::Value::Dict params;
  params.Set("audioDebugRecordingsEnabled",
             webrtc_internals_->IsAudioDebugRecordingsEnabled());
  params.Set("eventLogRecordingsEnabled",
             webrtc_internals_->IsEventLogRecordingsEnabled());
  params.Set("eventLogRecordingsToggleable",
             webrtc_internals_->CanToggleEventLogRecordings());

  for (auto* host : PeerConnectionTrackerHost::GetAllHosts()) {
    host->GetCurrentState();
  }
  ResolveJavascriptCallback(base::Value(callback_id), params);
}

void WebRTCInternalsMessageHandler::OnUpdate(const std::string& event_name,
                                             const base::Value* event_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsJavascriptAllowed()) {
    // Javascript is disallowed, either due to the page still loading, or in the
    // process of being unloaded. Skip this update.
    return;
  }

  RenderFrameHost* host = GetWebRTCInternalsHost();
  if (!host)
    return;

  if (event_data) {
    FireWebUIListener(event_name, *event_data);
  } else {
    FireWebUIListener(event_name, base::Value());
  }
}

}  // namespace content
