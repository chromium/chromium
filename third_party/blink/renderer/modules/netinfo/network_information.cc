// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/netinfo/network_information.h"

#include <algorithm>

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

Settings* GetSettings(ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;

  auto* document = DynamicTo<Document>(execution_context);
  if (!document)
    return nullptr;

  // |document| is guaranteed to be non-null since |execution_context| is
  // non-null.
  return document->GetSettings();
}

bool IsInDataSaverHoldbackWebApi(ExecutionContext* execution_context) {
  Settings* settings = GetSettings(execution_context);
  if (!settings)
    return false;
  return settings->GetDataSaverHoldbackWebApi();
}

String ConnectionTypeToString(WebConnectionType type) {
  switch (type) {
    case kWebConnectionTypeCellular2G:
    case kWebConnectionTypeCellular3G:
    case kWebConnectionTypeCellular4G:
      return "cellular";
    case kWebConnectionTypeBluetooth:
      return "bluetooth";
    case kWebConnectionTypeEthernet:
      return "ethernet";
    case kWebConnectionTypeWifi:
      return "wifi";
    case kWebConnectionTypeWimax:
      return "wimax";
    case kWebConnectionTypeOther:
      return "other";
    case kWebConnectionTypeNone:
      return "none";
    case kWebConnectionTypeUnknown:
      return "unknown";
  }
  NOTREACHED();
  return "none";
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

String NetworkInformation::type() const {
  // type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!IsObserving())
    return ConnectionTypeToString(GetNetworkStateNotifier().ConnectionType());

  // If observing, return m_type which changes when the event fires, per spec.
  return ConnectionTypeToString(type_);
}

double NetworkInformation::downlinkMax() const {
  if (!IsObserving())
    return GetNetworkStateNotifier().MaxBandwidth();

  return downlink_max_mbps_;
}

String NetworkInformation::effectiveType() {
  MaybeShowWebHoldbackConsoleMsg();
  base::Optional<WebEffectiveConnectionType> override_ect =
      GetNetworkStateNotifier().GetWebHoldbackEffectiveType();
  if (override_ect) {
    return NetworkStateNotifier::EffectiveConnectionTypeToString(
        override_ect.value());
  }

  // effective_type_ is only updated when listening for events, so ask
  // networkStateNotifier if not listening (crbug.com/379841).
  if (!IsObserving()) {
    return NetworkStateNotifier::EffectiveConnectionTypeToString(
        GetNetworkStateNotifier().EffectiveType());
  }

  // If observing, return m_type which changes when the event fires, per spec.
  return NetworkStateNotifier::EffectiveConnectionTypeToString(effective_type_);
}

uint32_t NetworkInformation::rtt() {
  MaybeShowWebHoldbackConsoleMsg();
  base::Optional<base::TimeDelta> override_rtt =
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
  base::Optional<double> override_downlink_mbps =
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
  return IsObserving()
             ? save_data_
             : GetNetworkStateNotifier().SaveDataEnabled() &&
                   !IsInDataSaverHoldbackWebApi(GetExecutionContext());
}

void NetworkInformation::ConnectionChange(
    WebConnectionType type,
    double downlink_max_mbps,
    WebEffectiveConnectionType effective_type,
    const base::Optional<base::TimeDelta>& http_rtt,
    const base::Optional<base::TimeDelta>& transport_rtt,
    const base::Optional<double>& downlink_mbps,
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
  return ContextLifecycleObserver::GetExecutionContext();
}

void NetworkInformation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  MaybeShowWebHoldbackConsoleMsg();
  StartObserving();
}

void NetworkInformation::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void NetworkInformation::RemoveAllEventListeners() {
  EventTargetWithInlineData::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

bool NetworkInformation::HasPendingActivity() const {
  DCHECK(context_stopped_ || IsObserving() == HasEventListeners());

  // Prevent collection of this object when there are active listeners.
  return IsObserving();
}

void NetworkInformation::ContextDestroyed(ExecutionContext*) {
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

NetworkInformation::NetworkInformation(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      web_holdback_console_message_shown_(false),
      context_stopped_(false) {
  base::Optional<base::TimeDelta> http_rtt;
  base::Optional<double> downlink_mbps;

  GetNetworkStateNotifier().GetMetricsWithWebHoldback(
      &type_, &downlink_max_mbps_, &effective_type_, &http_rtt, &downlink_mbps,
      &save_data_);

  http_rtt_msec_ = GetNetworkStateNotifier().RoundRtt(Host(), http_rtt);
  downlink_mbps_ = GetNetworkStateNotifier().RoundMbps(Host(), downlink_mbps);
  save_data_ =
      save_data_ && !IsInDataSaverHoldbackWebApi(GetExecutionContext());

  DCHECK_LE(1u, GetNetworkStateNotifier().RandomizationSalt());
  DCHECK_GE(20u, GetNetworkStateNotifier().RandomizationSalt());
}

void NetworkInformation::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const String NetworkInformation::Host() const {
  return GetExecutionContext() ? GetExecutionContext()->Url().Host() : String();
}

void NetworkInformation::MaybeShowWebHoldbackConsoleMsg() {
  if (web_holdback_console_message_shown_)
    return;
  web_holdback_console_message_shown_ = true;
  if (!GetNetworkStateNotifier().GetWebHoldbackEffectiveType())
    return;
  GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      GetConsoleLogStringForWebHoldback()));
}

}  // namespace blink
