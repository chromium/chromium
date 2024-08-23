// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_IOS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_IOS_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#import "components/browser_sync/sync_internals_message_handler.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncInvalidationsService;
class SyncService;
class UserEventService;
}  // namespace syncer

// iOS-specific implementation of SyncInternalsMessageHandler.
class IOSSyncInternalsMessageHandler
    : public browser_sync::SyncInternalsMessageHandler::Delegate,
      public web::WebUIIOSMessageHandler {
 public:
  IOSSyncInternalsMessageHandler(
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::UserEventService* user_event_service,
      const std::string& channel);

  IOSSyncInternalsMessageHandler(const IOSSyncInternalsMessageHandler&) =
      delete;
  IOSSyncInternalsMessageHandler& operator=(
      const IOSSyncInternalsMessageHandler&) = delete;

  ~IOSSyncInternalsMessageHandler() override = default;

  // browser_sync::SyncInternalsMessageHandler overrides.
  void SendEventToPage(std::string_view event_name,
                       base::span<const base::ValueView> args) override;
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

  // web::WebUIIOSMessageHandler overrides.
  void RegisterMessages() override;

 private:
  browser_sync::SyncInternalsMessageHandler message_handler_;
};

#endif  // IOS_COMPONENTS_WEBUI_SYNC_INTERNALS_IOS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
