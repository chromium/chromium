// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NETINFO_NETWORK_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NETINFO_NETWORK_INFORMATION_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

class ExecutionContext;

class NetworkInformation final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<NetworkInformation>,
      public ContextLifecycleObserver,
      public NetworkStateNotifier::NetworkStateObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NetworkInformation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NetworkInformation(ExecutionContext*);
  ~NetworkInformation() override;

  String type() const;
  double downlinkMax() const;
  String effectiveType();
  uint32_t rtt();
  double downlink();
  bool saveData() const;

  // NetworkStateObserver overrides.
  void ConnectionChange(WebConnectionType,
                        double downlink_max_mbps,
                        WebEffectiveConnectionType effective_type,
                        const base::Optional<base::TimeDelta>& http_rtt,
                        const base::Optional<base::TimeDelta>& transport_rtt,
                        const base::Optional<double>& downlink_mbps,
                        bool save_data) override;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*) override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(typechange, kTypechange)  // Deprecated

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  void StartObserving();
  void StopObserving();

  // Whether this object is listening for events from NetworkStateNotifier.
  bool IsObserving() const;

  const String Host() const;

  void MaybeShowWebHoldbackConsoleMsg();

  // Touched only on context thread.
  WebConnectionType type_;

  // Touched only on context thread.
  double downlink_max_mbps_;

  // Current effective connection type, which is the connection type whose
  // typical performance is most similar to the measured performance of the
  // network in use.
  WebEffectiveConnectionType effective_type_;

  // HTTP RTT estimate. Rounded off to the nearest 25 msec. Touched only on
  // context thread.
  uint32_t http_rtt_msec_;

  // Downlink throughput estimate. Rounded off to the nearest 25 kbps. Touched
  // only on context thread.
  double downlink_mbps_;

  // Whether the data saving mode is enabled.
  bool save_data_;

  // True if the console message indicating that network quality is overridden
  // using a holdback experiment has been shown. Set to true if the console
  // message has been shown, or if the holdback experiment is not enabled.
  bool web_holdback_console_message_shown_;

  // Whether ContextLifecycleObserver::contextDestroyed has been called.
  bool context_stopped_;

  std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
      connection_observer_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NETINFO_NETWORK_INFORMATION_H_
