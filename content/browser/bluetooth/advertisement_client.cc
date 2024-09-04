// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/advertisement_client.h"

#include <utility>
#include <vector>

#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"

namespace content {

namespace {

using ::device::BluetoothUUID;

}

WebBluetoothServiceImpl::AdvertisementClient::AdvertisementClient(
    WebBluetoothServiceImpl* service,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    RequestCallback callback)
    : client_remote_(std::move(client_remote)),
      web_contents_(static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(&service->render_frame_host()))),
      service_(service),
      callback_(std::move(callback)) {
  // Using base::Unretained() is safe here because all instances of this class
  // will be owned by |service|.
  client_remote_.set_disconnect_handler(
      base::BindOnce(&WebBluetoothServiceImpl::RemoveDisconnectedClients,
                     base::Unretained(service)));
  web_contents_->IncrementBluetoothScanningSessionsCount();
}

WebBluetoothServiceImpl::AdvertisementClient::~AdvertisementClient() {
  web_contents_->DecrementBluetoothScanningSessionsCount();
}

WebBluetoothServiceImpl::WatchAdvertisementsClient::WatchAdvertisementsClient(
    WebBluetoothServiceImpl* service,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    blink::WebBluetoothDeviceId device_id,
    RequestCallback callback)
    : AdvertisementClient(service,
                          std::move(client_remote),
                          std::move(callback)),
      device_id_(device_id) {
  DCHECK(device_id_.IsValid());
}

WebBluetoothServiceImpl::WatchAdvertisementsClient::
    ~WatchAdvertisementsClient() = default;

void WebBluetoothServiceImpl::WatchAdvertisementsClient::SendEvent(
    const blink::mojom::WebBluetoothAdvertisingEvent& event) {
  if (event.device->id != device_id_) {
    return;
  }

  auto filtered_event = event.Clone();
  std::erase_if(filtered_event->uuids, [this](const BluetoothUUID& uuid) {
    return !service_->IsAllowedToAccessService(device_id_, uuid);
  });
  base::EraseIf(
      filtered_event->service_data,
      [this](const std::pair<BluetoothUUID, std::vector<uint8_t>>& entry) {
        return !service_->IsAllowedToAccessService(device_id_, entry.first);
      });
  base::EraseIf(filtered_event->manufacturer_data,
                [this](const std::pair<blink::mojom::WebBluetoothCompanyPtr,
                                       std::vector<uint8_t>>& entry) {
                  return !service_->IsAllowedToAccessManufacturerData(
                             device_id_, entry.first->id) ||
                         BluetoothBlocklist::Get().IsExcluded(entry.first->id,
                                                              entry.second);
                });
  client_remote_->AdvertisingEvent(std::move(filtered_event));
}

WebBluetoothServiceImpl::ScanningClient::ScanningClient(
    WebBluetoothServiceImpl* service,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestCallback callback)
    : AdvertisementClient(service,
                          std::move(client_remote),
                          std::move(callback)),
      options_(std::move(options)) {
  DCHECK(options_->filters.has_value() || options_->accept_all_advertisements);
}

WebBluetoothServiceImpl::ScanningClient::~ScanningClient() = default;

void WebBluetoothServiceImpl::ScanningClient::SendEvent(
    const blink::mojom::WebBluetoothAdvertisingEvent& event) {
  // TODO(crbug.com/40707749): Filter out advertisement data if not
  // included in the filters, optionalServices, or optionalManufacturerData.
  auto filtered_event = event.Clone();
  if (options_->accept_all_advertisements) {
    if (prompt_controller_) {
      AddFilteredDeviceToPrompt(filtered_event->device->id.str(),
                                filtered_event->name);
    }

    if (allow_send_event_) {
      client_remote_->AdvertisingEvent(std::move(filtered_event));
    }

    return;
  }

  DCHECK(options_->filters.has_value());

  // For every filter, we're going to check to see if a |name|, |name_prefix|,
  // or |services| have been set. If one of these is set, we will check the
  // scan result to see if it matches the filter's value.  If it doesn't,
  // we'll just continue with the next filter. If all of the properties in a
  // filter have a match, we can post the AdvertisingEvent. Otherwise, we are
  // going to drop it. This logic can be reduced a bit, but I think clarity
  // will decrease.
  for (auto& filter : options_->filters.value()) {
    // Check to see if there is a direct match against the advertisement name
    if (filter->name.has_value()) {
      if (!filtered_event->name.has_value() ||
          filter->name.value() != filtered_event->name.value()) {
        continue;
      }
    }

    // Check if there is a name prefix match
    if (filter->name_prefix.has_value()) {
      if (!filtered_event->name.has_value() ||
          !base::StartsWith(filtered_event->name.value(),
                            filter->name_prefix.value(),
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    }

    // Check to see if there is a service uuid match
    if (filter->services.has_value()) {
      if (base::ranges::none_of(
              filter->services.value(),
              [&filtered_event](const BluetoothUUID& filter_uuid) {
                return base::Contains(filtered_event->uuids, filter_uuid);
              })) {
        continue;
      }
    }

    // TODO(crbug.com/41310835): Support manufacturerData and serviceData
    // filters.

    if (prompt_controller_) {
      AddFilteredDeviceToPrompt(filtered_event->device->id.str(),
                                filtered_event->name);
    }

    if (allow_send_event_) {
      client_remote_->AdvertisingEvent(std::move(filtered_event));
    }
    return;
  }
}

}  // namespace content
