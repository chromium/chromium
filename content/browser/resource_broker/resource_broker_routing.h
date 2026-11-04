// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_ROUTING_H_
#define CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_ROUTING_H_

namespace net {
class IsolationInfo;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;

// Whether a document's subresource `URLLoaderFactory` is eligible for routing
// through the Resource Broker and, if not, why. Logged to
// ResourceBroker.RoutingDecision.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ResourceBrokerRoutingDecision)
enum class ResourceBrokerRoutingDecision {
  kRouted = 0,
  kErrorPage = 1,
  kOpaqueOrigin = 2,
  kWebUI = 3,
  kExtension = 4,
  kIsolatedWebApp = 5,
  kSchemeNotHttpOrHttps = 6,
  kNotLockedToSite = 7,
  kPdf = 8,
  kGuest = 9,
  kPrivileged = 10,
  kCrossOriginIsolated = 11,
  kServiceWorkerControlled = 12,
  kNoncePartitioned = 13,
  kMaxValue = kNoncePartitioned,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/resource_broker/enums.xml:ResourceBrokerRoutingDecision)

// Evaluates and records whether a document's network-backed subresource
// `URLLoaderFactory` is eligible for routing through the Resource Broker.
// Called on the UI thread whenever such a factory is created (`frame` may be
// speculative during navigation commit). `frame` must not be null.
ResourceBrokerRoutingDecision RecordResourceBrokerRoutingDecision(
    RenderFrameHost* frame,
    const url::Origin& origin_lock,
    const net::IsolationInfo& isolation_info);

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_BROKER_RESOURCE_BROKER_ROUTING_H_
