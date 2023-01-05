// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/log_handler.h"

#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {
namespace protocol {

LogHandler::LogHandler() : DevToolsDomainHandler(Log::Metainfo::domainName) {}
LogHandler::~LogHandler() = default;

// static
std::vector<LogHandler*> LogHandler::ForAgentHost(DevToolsAgentHostImpl* host) {
  return host->HandlersByName<LogHandler>(Log::Metainfo::domainName);
}

void LogHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Log::Frontend>(dispatcher->channel());
  Log::Dispatcher::wire(dispatcher, this);
}

DispatchResponse LogHandler::Disable() {
  enabled_ = false;
  return Response::FallThrough();
}

DispatchResponse LogHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

void LogHandler::EntryAdded(Log::LogEntry* entry) {
  if (!enabled_) {
    return;
  }
  frontend_->EntryAdded(entry->Clone());
}

}  // namespace protocol
}  // namespace content
