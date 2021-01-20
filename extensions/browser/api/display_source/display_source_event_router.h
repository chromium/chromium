// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Observe listeners to 'onSinksUpdated' events.
// This class initiates/stops listening for the available sinks
// by the DisplaySourceConnectionDelegate, depending on whether
// there are existing listeners to 'onSinksUpdated' event.
class DisplaySourceEventRouter
    : public KeyedService,
      public EventRouter::Observer,
      public DisplaySourceConnectionDelegate::Observer {
 public:
  static DisplaySourceEventRouter* Create(
      content::BrowserContext* browser_context);

  ~DisplaySourceEventRouter() override;

 private:
  explicit DisplaySourceEventRouter(content::BrowserContext* browser_context);

  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // DisplaySourceConnectionDelegate::Observer overrides:
  void OnSinksUpdated(const DisplaySourceSinkInfoList& sinks) override;
  void OnConnectionError(int sink_id,
                         DisplaySourceErrorType type,
                         const std::string& description) override {}

 private:
  void StartOrStopListeningForSinksChanges();

  content::BrowserContext* browser_context_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySourceEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_EVENT_ROUTER_H_
