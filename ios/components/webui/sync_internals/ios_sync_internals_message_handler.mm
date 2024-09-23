// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/sync_internals/ios_sync_internals_message_handler.h"

#import <string>
#import <vector>

#import "base/containers/span.h"
#import "base/ranges/algorithm.h"
#import "ios/web/public/webui/web_ui_ios.h"

IOSSyncInternalsMessageHandler::IOSSyncInternalsMessageHandler(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service,
    syncer::UserEventService* user_event_service,
    const std::string& channel)
    : message_handler_(this,
                       identity_manager,
                       sync_service,
                       sync_invalidations_service,
                       user_event_service,
                       channel) {}

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
  for (const auto& [message, handler] :
       message_handler_.GetMessageHandlerMap()) {
    web_ui()->RegisterMessageCallback(message, handler);
  }
}
