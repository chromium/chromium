// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_location_notifier.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/wifi_data.h"
#include "services/device/public/cpp/geolocation/network_location_request_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace device {

namespace {
// Time to wait before issuing a network geolocation request in response to
// network change notification. Network changes tend to occur in clusters.
constexpr base::TimeDelta kNetworkChangeReactionDelay = base::Minutes(5);
}  // namespace

PublicIpAddressLocationNotifier::PublicIpAddressLocationNotifier(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const std::string& api_key)
    : network_changed_since_last_request_(true),
      api_key_(api_key),
      url_loader_factory_(url_loader_factory),
      network_connection_tracker_(network_connection_tracker),
      network_traffic_annotation_tag_(nullptr) {
  // Subscribe to notifications of changes in network configuration.
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

PublicIpAddressLocationNotifier::~PublicIpAddressLocationNotifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void PublicIpAddressLocationNotifier::QueryNextPosition(
    base::Time time_of_prev_position,
    const net::PartialNetworkTrafficAnnotationTag& tag,
    QueryNextPositionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  network_traffic_annotation_tag_ =
      std::make_unique<net::PartialNetworkTrafficAnnotationTag>(tag);
  // If a network location request is in flight, wait.
  if (network_location_request_) {
    callbacks_.push_back(std::move(callback));
    return;
  }

  // If a network change has occured since we last made a request, start a
  // request and wait.
  if (network_changed_since_last_request_) {
    callbacks_.push_back(std::move(callback));
    MakeNetworkLocationRequest();
    return;
  }

  if (latest_result_ && latest_result_->is_position() &&
      latest_result_->get_position()->timestamp > time_of_prev_position) {
    std::move(callback).Run(latest_result_.Clone());
    return;
  }

  // The cached geoposition is not new enough for this client, and
  // there hasn't been a recent network change, so add the client
  // to the list of clients waiting for a network change.
  callbacks_.push_back(std::move(callback));
}

void PublicIpAddressLocationNotifier::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Post a cancelable task to react to this network change after a reasonable
  // delay, so that we only react once if multiple network changes occur in a
  // short span of time.
  react_to_network_change_closure_.Reset(
      base::BindOnce(&PublicIpAddressLocationNotifier::ReactToNetworkChange,
                     base::Unretained(this)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, react_to_network_change_closure_.callback(),
      kNetworkChangeReactionDelay);
}

void PublicIpAddressLocationNotifier::ReactToNetworkChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_changed_since_last_request_ = true;

  // Invalidate the cached recent position.
  latest_result_.reset();

  // If any clients are waiting, start a request.
  // (This will cancel any previous request, which is OK.)
  if (!callbacks_.empty())
    MakeNetworkLocationRequest();
}

void PublicIpAddressLocationNotifier::MakeNetworkLocationRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_changed_since_last_request_ = false;
  if (!url_loader_factory_)
    return;

  network_location_request_ = std::make_unique<NetworkLocationRequest>(
      url_loader_factory_, api_key_,
      base::BindRepeating(
          &PublicIpAddressLocationNotifier::OnNetworkLocationResponse,
          weak_ptr_factory_.GetWeakPtr()));

  DCHECK(network_traffic_annotation_tag_);
  network_location_request_->MakeRequest(
      WifiData(), base::Time::Now(), *network_traffic_annotation_tag_,
      NetworkLocationRequestSource::kPublicIpAddressGeolocator);
}

void PublicIpAddressLocationNotifier::OnNetworkLocationResponse(
    mojom::GeopositionResultPtr result,
    const bool server_error,
    const WifiData& /* wifi_data */,
    mojom::NetworkLocationResponsePtr /* response data */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (server_error) {
    network_changed_since_last_request_ = true;
    DCHECK(!latest_result_);
  } else {
    latest_result_ = result.Clone();
  }
  // Notify all clients.
  for (QueryNextPositionCallback& callback : callbacks_)
    std::move(callback).Run(result.Clone());
  callbacks_.clear();
  network_location_request_.reset();
}

}  // namespace device
