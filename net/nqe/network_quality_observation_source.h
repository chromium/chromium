// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_OBSERVATION_SOURCE_H_
#define NET_NQE_NETWORK_QUALITY_OBSERVATION_SOURCE_H_

namespace net {

// On Android, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: NetworkQualityObservationSource
// GENERATED_JAVA_PREFIX_TO_STRIP: NETWORK_QUALITY_OBSERVATION_SOURCE_
enum NetworkQualityObservationSource {
  // The observation was taken at the request layer, e.g., a round trip time
  // is recorded as the time between the request being sent and the first byte
  // being received.
  NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP = 0,

  // The observation is taken from TCP statistics maintained by the kernel.
  NETWORK_QUALITY_OBSERVATION_SOURCE_TCP = 1,

  // The observation is taken at the QUIC layer.
  NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC = 2,

  // The observation is a previously cached estimate of the metric.  The metric
  // was computed at the HTTP layer.
  NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE = 3,

  // The observation is derived from network connection information provided
  // by the platform. For example, typical RTT and throughput values are used
  // for a given type of network connection.  The metric was provided for use
  // at the HTTP layer.
  NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM = 4,

  // The observation came from a Chromium-external source. The metric was
  // computed by the external source at the HTTP layer.
  // Deprecated since external estimate provider is not currently queried.
  DEPRECATED_NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_EXTERNAL_ESTIMATE = 5,

  // The observation is a previously cached estimate of the metric. The metric
  // was computed at the transport layer.
  NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE = 6,

  // The observation is derived from the network connection information provided
  // by the platform. For example, typical RTT and throughput values are used
  // for a given type of network connection.  The metric was provided for use
  // at the transport layer.
  NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_TRANSPORT_FROM_PLATFORM = 7,

  // Round trip ping latency reported by H2 connections.
  NETWORK_QUALITY_OBSERVATION_SOURCE_H2_PINGS = 8,

  NETWORK_QUALITY_OBSERVATION_SOURCE_MAX,
};

namespace nqe::internal {

// Different categories to which an observation source can belong to. Each
// observation source belongs to exactly one category.
enum ObservationCategory {
  // HTTP RTT observations measure the RTT from this device taken at the
  // HTTP layer. If a HTTP-layer proxy is in use, then the RTT observations
  // would measure the RTT from this device to the proxy.
  OBSERVATION_CATEGORY_HTTP = 0,
  // Transport RTT observations measure the RTT from this device taken at the
  // transport layer. If a transport-layer proxy (e.g., TCP proxy) is in use,
  // then the RTT observations would measure the RTT from this device to the
  // proxy.
  OBSERVATION_CATEGORY_TRANSPORT = 1,
  // End to end RTT observations measure the RTT from this device to the remote
  // web server. Currently, this only includes RTT observations taken from the
  // QUIC connections.
  OBSERVATION_CATEGORY_END_TO_END = 2,
  OBSERVATION_CATEGORY_COUNT = 3
};

}  // namespace nqe::internal

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_OBSERVATION_SOURCE_H_
