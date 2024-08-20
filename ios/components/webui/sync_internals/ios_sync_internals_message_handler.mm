// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/sync_internals/ios_sync_internals_message_handler.h"

#import <string>
#import <vector>

#import "base/containers/span.h"
#import "base/ranges/algorithm.h"
#import "ios/components/webui/web_ui_provider.h"
#import "ios/web/public/webui/web_ui_ios.h"

syncer::SyncService* IOSSyncInternalsMessageHandler::GetSyncService() {
  return web_ui::GetSyncServiceForWebUI(web_ui());
}

syncer::SyncInvalidationsService*
IOSSyncInternalsMessageHandler::GetSyncInvalidationsService() {
  return web_ui::GetSyncInvalidationsServiceForWebUI(web_ui());
}

syncer::UserEventService*
IOSSyncInternalsMessageHandler::GetUserEventService() {
  return web_ui::GetUserEventServiceForWebUI(web_ui());
}

std::string IOSSyncInternalsMessageHandler::GetChannel() {
  return web_ui::GetChannelString();
}

void IOSSyncInternalsMessageHandler::SendEventToPage(
    std::string_view event_name,
    base::span<const base::ValueView> args) {
  std::vector<base::ValueView> event_name_and_args;
  event_name_and_args.push_back(event_name);
  base::ranges::copy(args, std::back_inserter(event_name_and_args));
  base::span<base::ValueView> mutable_span(event_name_and_args);
  // `mutable_span` will be implicitly converted to a const one. Declaring
  // std::vector<const base::ValueView> above is not an option, because
  // vector elements need to be mutable.
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback",
                                   std::move(mutable_span));
}

void IOSSyncInternalsMessageHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

void IOSSyncInternalsMessageHandler::RegisterMessages() {
  for (const auto& [message, handler] : GetMessageHandlerMap()) {
    web_ui()->RegisterMessageCallback(message, handler);
  }
}
