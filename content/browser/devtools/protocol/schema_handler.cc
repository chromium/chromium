// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/schema_handler.h"


namespace content {
namespace protocol {

SchemaHandler::SchemaHandler()
    : DevToolsDomainHandler(Schema::Metainfo::domainName) {
}

SchemaHandler::~SchemaHandler() = default;

void SchemaHandler::Wire(UberDispatcher* dispatcher) {
  Schema::Dispatcher::wire(dispatcher, this);
}

Response SchemaHandler::GetDomains(
    std::unique_ptr<protocol::Array<Schema::Domain>>* domains) {
  // TODO(kozyatisnkiy): get this from the target instead of hardcoding a list.
  static const char kVersion[] = "1.2";
  static const char* kDomains[] = {
      "Inspector",     "Memory",     "Page",          "Emulation",
      "Security",      "Network",    "Database",      "IndexedDB",
      "CacheStorage",  "DOMStorage", "CSS",           "ApplicationCache",
      "DOM",           "IO",         "DOMDebugger",   "DOMSnapshot",
      "ServiceWorker", "Input",      "LayerTree",     "DeviceOrientation",
      "Tracing",       "Animation",  "Accessibility", "Storage",
      "Log",           "Runtime",    "Debugger",      "Profiler",
      "HeapProfiler",  "Schema",     "Target",        "Overlay",
      "Performance",   "Audits",     "HeadlessExperimental"};
  *domains = std::make_unique<protocol::Array<Schema::Domain>>();
  for (const char* domain : kDomains) {
    (*domains)->emplace_back(
        Schema::Domain::Create().SetName(domain).SetVersion(kVersion).Build());
  }
  return Response::Success();
}

}  // namespace protocol
}  // namespace content
