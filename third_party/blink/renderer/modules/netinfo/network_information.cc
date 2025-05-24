// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/netinfo/network_information.h"

#include <algorithm>

#include "base/time/time.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_connection_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

V8ConnectionType::Enum ConnectionTypeToEnum(WebConnectionType type) {
  switch (type) {
    case kWebConnectionTypeCellular2G:
    case kWebConnectionTypeCellular3G:
    case kWebConnectionTypeCellular4G:
      return V8ConnectionType::Enum::kCellular;
    case kWebConnectionTypeBluetooth:
      return V8ConnectionType::Enum::kBluetooth;
    case kWebConnectionTypeEthernet:
      return V8ConnectionType::Enum::kEthernet;
    case kWebConnectionTypeWifi:
      return V8ConnectionType::Enum::kWifi;
    case kWebConnectionTypeWimax:
      return V8ConnectionType::Enum::kWimax;
    case kWebConnectionTypeOther:
      return V8ConnectionType::Enum::kOther;
    case kWebConnectionTypeNone:
      return V8ConnectionType::Enum::kNone;
    case kWebConnectionTypeUnknown:
      return V8ConnectionType::Enum::kUnknown;
  }
  NOTREACHED();
}

V8EffectiveConnectionType::Enum EffectiveConnectionTypeToEnum(
    WebEffectiveConnectionType type) {
  switch (type) {
    case WebEffectiveConnectionType::kTypeSlow2G:
      return V8EffectiveConnectionType::Enum::kSlow2G;
    case WebEffectiveConnectionType::kType2G:
      return V8EffectiveConnectionType::Enum::k2G;
    case WebEffectiveConnectionType::kType3G:
      return V8EffectiveConnectionType::Enum::k3G;
    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
    case WebEffectiveConnectionType::kType4G:
      return V8EffectiveConnectionType::Enum::k4G;
  }
  NOTREACHED();
}

String GetConsoleLogStringForWebHoldback() {
  return "Network quality values are overridden using a holdback experiment, "
         "and so may be inaccurate";
}

}  // namespace

NetworkInformation::~NetworkInformation() {
  DCHECK(!IsObserving());
}

bool NetworkInformation::IsObserving() const {
  return !!connection_observer_handle_;
}

V8ConnectionType NetworkInformation::type() const {
  if (RuntimeEnabledFeatures::NetInfoConstantTypeEnabled()) {
    return V8ConnectionType(V8ConnectionType::Enum::kUnknown);
  }

  // type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!IsObserving()) {
    return V8ConnectionType(
        ConnectionTypeToEnum(GetNetworkStateNotifier().ConnectionType()));
  }

  // If observing, return m_type which changes when the event fires, per spec.
  return V8ConnectionType(ConnectionTypeToEnum(type_));
}

double NetworkInformation::downlinkMax() const {
  if (RuntimeEnabledFeatures::NetInfoConstantTypeEnabled()) {
    return std::numeric_limits<double>::infinity();
  }

  if (!IsObserving())
    return GetNetworkStateNotifier().MaxBandwidth();

  return downlink_max_mbps_;
}

V8EffectiveConnectionType NetworkInformation::effectiveType() {
  MaybeShowWebHoldbackConsoleMsg();
  std::optional<WebEffectiveConnectionType> override_ect =
      GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
  if (override_ect) {
    return V8EffectiveConnectionType(
        EffectiveConnectionTypeToEnum(override_ect.value()));
  }

  // effective_type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!IsObserving()) {
    return V8EffectiveConnectionType(EffectiveConnectionTypeToEnum(
        GetNetworkStateNotifier().EffectiveType()));
  }

  // If observing, return m_type which changes when the event fires, per spec.
  return V8EffectiveConnectionType(
      EffectiveConnectionTypeToEnum(effective_type_));
}

uint32_t NetworkInformation::rtt() {
  MaybeShowWebHoldbackConsoleMsg();
  std::optional<base::TimeDelta> override_rtt =
      GetNetworkStateNotifier().GetWebHoldbackHttpRtt();
  if (override_rtt) {
    return GetNetworkStateNotifier().RoundRtt(Host(), override_rtt.value());
  }

  if (!IsObserving()) {
    return GetNetworkStateNotifier().RoundRtt(
        Host(), GetNetworkStateNotifier().HttpRtt());
  }

  return http_rtt_msec_;
}

double NetworkInformation::downlink() {
  MaybeShowWebHoldbackConsoleMsg();
  std::optional<double> override_downlink_mbps =
      GetNetworkStateNotifier().GetWebHoldbackDownlinkThroughputMbps();
  if (override_downlink_mbps) {
    return GetNetworkStateNotifier().RoundMbps(Host(),
                                               override_downlink_mbps.value());
  }

  if (!IsObserving()) {
    return GetNetworkStateNotifier().RoundMbps(
        Host(), GetNetworkStateNotifier().DownlinkThroughputMbps());
  }

  return downlink_mbps_;
}

bool NetworkInformation::saveData() const {
  return IsObserving() ? save_data_
                       : GetNetworkStateNotifier().SaveDataEnabled();
}

void NetworkInformation::ConnectionChange(
    WebConnectionType type,
    double downlink_max_mbps,
    WebEffectiveConnectionType effective_type,
    const std::optional<base::TimeDelta>& http_rtt,
    const std::optional<base::TimeDelta>& transport_rtt,
    const std::optional<double>& downlink_mbps,
    bool save_data) {
  DCHECK(GetExecutionContext()->IsContextThread());

  const String host = Host();
  uint32_t new_http_rtt_msec =
      GetNetworkStateNotifier().RoundRtt(host, http_rtt);
  double new_downlink_mbps =
      GetNetworkStateNotifier().RoundMbps(host, downlink_mbps);

  bool network_quality_estimate_changed = false;
  // Allow setting |network_quality_estimate_changed| to true only if the
  // network quality holdback experiment is not enabled.
  if (!GetNetworkStateNotifier().GetWebHoldbackEffectiveType()) {
    network_quality_estimate_changed = effective_type_ != effective_type ||
                                       http_rtt_msec_ != new_http_rtt_msec ||
                                       downlink_mbps_ != new_downlink_mbps;
  }

  // This can happen if the observer removes and then adds itself again
  // during notification, or if |transport_rtt| was the only metric that
  // changed.
  if (type_ == type && downlink_max_mbps_ == downlink_max_mbps &&
      !network_quality_estimate_changed && save_data_ == save_data) {
    return;
  }

  // If the NetInfoDownlinkMaxEnabled is not enabled, then |type| and
  // |downlink_max_mbps| should not be checked for change.
  if (!RuntimeEnabledFeatures::NetInfoDownlinkMaxEnabled() &&
      !network_quality_estimate_changed && save_data_ == save_data) {
    return;
  }

  bool type_changed =
      RuntimeEnabledFeatures::NetInfoDownlinkMaxEnabled() &&
      (type_ != type || downlink_max_mbps_ != downlink_max_mbps);

  type_ = type;
  downlink_max_mbps_ = downlink_max_mbps;
  if (network_quality_estimate_changed) {
    effective_type_ = effective_type;
    http_rtt_msec_ = new_http_rtt_msec;
    downlink_mbps_ = new_downlink_mbps;
  }
  save_data_ = save_data;

  if (type_changed)
    DispatchEvent(*Event::Create(event_type_names::kTypechange));
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

const AtomicString& NetworkInformation::InterfaceName() const {
  return event_target_names::kNetworkInformation;
}

ExecutionContext* NetworkInformation::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void NetworkInformation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  MaybeShowWebHoldbackConsoleMsg();
  StartObserving();
}

void NetworkInformation::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void NetworkInformation::RemoveAllEventListeners() {
  EventTarget::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

bool NetworkInformation::HasPendingActivity() const {
  DCHECK(context_stopped_ || IsObserving() == HasEventListeners());

  // Prevent collection of this object when there are active listeners.
  return IsObserving();
}

void NetworkInformation::ContextDestroyed() {
  context_stopped_ = true;
  StopObserving();
}

void NetworkInformation::StartObserving() {
  if (!IsObserving() && !context_stopped_) {
    type_ = GetNetworkStateNotifier().ConnectionType();
    DCHECK(!connection_observer_handle_);
    connection_observer_handle_ =
        GetNetworkStateNotifier().AddConnectionObserver(
            this, GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  }
}

void NetworkInformation::StopObserving() {
  if (IsObserving()) {
    DCHECK(connection_observer_handle_);
    connection_observer_handle_ = nullptr;
  }
}

const char NetworkInformation::kSupplementName[] = "NetworkInformation";

NetworkInformation* NetworkInformation::connection(NavigatorBase& navigator) {
  if (!navigator.GetExecutionContext())
    return nullptr;
  NetworkInformation* supplement =
      Supplement<NavigatorBase>::From<NetworkInformation>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NetworkInformation>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

NetworkInformation::NetworkInformation(NavigatorBase& navigator)
    : ActiveScriptWrappable<NetworkInformation>({}),
      Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      web_holdback_console_message_shown_(false),
      context_stopped_(false) {
  std::optional<base::TimeDelta> http_rtt;
  std::optional<double> downlink_mbps;

  GetNetworkStateNotifier().GetMetricsWithWebHoldback(
      &type_, &downlink_max_mbps_, &effective_type_, &http_rtt, &downlink_mbps,
      &save_data_);

  http_rtt_msec_ = GetNetworkStateNotifier().RoundRtt(Host(), http_rtt);
  downlink_mbps_ = GetNetworkStateNotifier().RoundMbps(Host(), downlink_mbps);

  DCHECK_LE(1u, GetNetworkStateNotifier().RandomizationSalt());
  DCHECK_GE(20u, GetNetworkStateNotifier().RandomizationSalt());
}

void NetworkInformation::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

const String NetworkInformation::Host() const {
  return GetExecutionContext() ? GetExecutionContext()->Url().Host().ToString()
                               : String();
}

void NetworkInformation::MaybeShowWebHoldbackConsoleMsg() {
  if (web_holdback_console_message_shown_)
    return;
  web_holdback_console_message_shown_ = true;
  if (!GetNetworkStateNotifier().GetWebHoldbackEffectiveType())
    return;
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      GetConsoleLogStringForWebHoldback()));
}

}  // namespace blink
