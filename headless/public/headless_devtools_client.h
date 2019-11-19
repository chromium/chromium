// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CLIENT_H_
#define HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "headless/public/headless_devtools_channel.h"
#include "headless/public/headless_export.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace headless {

namespace accessibility {
class Domain;
}
namespace animation {
class Domain;
}
namespace application_cache {
class Domain;
}
namespace browser {
class Domain;
}
namespace cache_storage {
class Domain;
}
namespace console {
class Domain;
}
namespace css {
class Domain;
}
namespace database {
class Domain;
}
namespace debugger {
class Domain;
}
namespace device_orientation {
class Domain;
}
namespace dom {
class Domain;
}
namespace dom_debugger {
class Domain;
}
namespace dom_snapshot {
class Domain;
}
namespace dom_storage {
class Domain;
}
namespace emulation {
class Domain;
}
namespace fetch {
class Domain;
}
namespace headless_experimental {
class Domain;
}
namespace heap_profiler {
class Domain;
}
namespace indexeddb {
class Domain;
}
namespace input {
class Domain;
}
namespace inspector {
class Domain;
}
namespace io {
class Domain;
}
namespace layer_tree {
class Domain;
}
namespace log {
class Domain;
}
namespace memory {
class Domain;
}
namespace network {
class Domain;
}
namespace page {
class Domain;
}
namespace performance {
class Domain;
}
namespace profiler {
class Domain;
}
namespace runtime {
class Domain;
}
namespace security {
class Domain;
}
namespace service_worker {
class Domain;
}
namespace target {
class Domain;
}
namespace tracing {
class Domain;
}

// An interface for controlling and receiving events from a devtools target.
class HEADLESS_EXPORT HeadlessDevToolsClient {
 public:
  virtual ~HeadlessDevToolsClient() {}

  class HEADLESS_EXPORT ExternalHost {
   public:
    ExternalHost() {}
    virtual ~ExternalHost() {}
    virtual void SendProtocolMessage(const std::string& message) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ExternalHost);
  };

  static std::unique_ptr<HeadlessDevToolsClient> Create();

  // TODO(dgozman): remove this method and ExternalHost altogether.
  static std::unique_ptr<HeadlessDevToolsClient> CreateWithExternalHost(
      ExternalHost*);

  // DevTools commands are split into domains which corresponds to the getters
  // below. Each domain can be used to send commands and to subscribe to events.
  //
  // See http://chromedevtools.github.io/debugger-protocol-viewer/ for
  // the capabilities of each domain.
  virtual accessibility::Domain* GetAccessibility() = 0;
  virtual animation::Domain* GetAnimation() = 0;
  virtual application_cache::Domain* GetApplicationCache() = 0;
  virtual browser::Domain* GetBrowser() = 0;
  virtual cache_storage::Domain* GetCacheStorage() = 0;
  virtual console::Domain* GetConsole() = 0;
  virtual css::Domain* GetCSS() = 0;
  virtual database::Domain* GetDatabase() = 0;
  virtual debugger::Domain* GetDebugger() = 0;
  virtual device_orientation::Domain* GetDeviceOrientation() = 0;
  virtual dom::Domain* GetDOM() = 0;
  virtual dom_debugger::Domain* GetDOMDebugger() = 0;
  virtual dom_snapshot::Domain* GetDOMSnapshot() = 0;
  virtual dom_storage::Domain* GetDOMStorage() = 0;
  virtual emulation::Domain* GetEmulation() = 0;
  virtual fetch::Domain* GetFetch() = 0;
  virtual headless_experimental::Domain* GetHeadlessExperimental() = 0;
  virtual heap_profiler::Domain* GetHeapProfiler() = 0;
  virtual indexeddb::Domain* GetIndexedDB() = 0;
  virtual input::Domain* GetInput() = 0;
  virtual inspector::Domain* GetInspector() = 0;
  virtual io::Domain* GetIO() = 0;
  virtual layer_tree::Domain* GetLayerTree() = 0;
  virtual log::Domain* GetLog() = 0;
  virtual memory::Domain* GetMemory() = 0;
  virtual network::Domain* GetNetwork() = 0;
  virtual page::Domain* GetPage() = 0;
  virtual performance::Domain* GetPerformance() = 0;
  virtual profiler::Domain* GetProfiler() = 0;
  virtual runtime::Domain* GetRuntime() = 0;
  virtual security::Domain* GetSecurity() = 0;
  virtual service_worker::Domain* GetServiceWorker() = 0;
  virtual target::Domain* GetTarget() = 0;
  virtual tracing::Domain* GetTracing() = 0;

  class HEADLESS_EXPORT RawProtocolListener {
   public:
    RawProtocolListener() {}
    virtual ~RawProtocolListener() {}

    // Returns true if the listener handled the message.
    virtual bool OnProtocolMessage(
        const std::string& json_message,
        const base::DictionaryValue& parsed_message) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(RawProtocolListener);
  };

  virtual void AttachToChannel(
      std::unique_ptr<HeadlessDevToolsChannel> channel) = 0;
  virtual void DetachFromChannel() = 0;

  virtual void SetRawProtocolListener(
      RawProtocolListener* raw_protocol_listener) = 0;

  virtual std::unique_ptr<HeadlessDevToolsClient> CreateSession(
      const std::string& session_id) = 0;

  // Generates an odd numbered ID.
  virtual int GetNextRawDevToolsMessageId() = 0;

  // The id within the message must be odd to prevent collisions.
  virtual void SendRawDevToolsMessage(const std::string& json_message) = 0;

  // TODO(dgozman): remove this method together with ExternalHost.
  virtual void DispatchMessageFromExternalHost(
      const std::string& json_message) = 0;

  // TODO(skyostil): Add notification for disconnection.

 private:
  friend class HeadlessDevToolsClientImpl;

  HeadlessDevToolsClient() {}

  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsClient);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_CLIENT_H_
