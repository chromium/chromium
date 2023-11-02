// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/log.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class DevToolsAgentHostImpl;

namespace protocol {

class LogHandler final : public DevToolsDomainHandler, public Log::Backend {
 public:
  LogHandler();

  LogHandler(const LogHandler&) = delete;
  LogHandler& operator=(const LogHandler&) = delete;

  ~LogHandler() override;

  static std::vector<LogHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // Log::Backend implementation.
  DispatchResponse Disable() override;
  DispatchResponse Enable() override;

  void EntryAdded(Log::LogEntry* entry);

 private:
  std::unique_ptr<Log::Frontend> frontend_;
  bool enabled_ = false;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_LOG_HANDLER_H_
