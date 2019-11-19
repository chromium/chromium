// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan.h"

#include "mojo/public/cpp/bindings/receiver_set.h"

namespace blink {

BluetoothLEScan::BluetoothLEScan(
    mojo::ReceiverId id,
    Bluetooth* bluetooth,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr options)
    : id_(id),
      bluetooth_(bluetooth),
      keep_repeated_devices_(options ? options->keep_repeated_devices : false),
      accept_all_advertisements_(options ? options->accept_all_advertisements
                                         : false) {
  DCHECK(options->filters.has_value() ^ options->accept_all_advertisements);

  if (options && options->filters.has_value()) {
    for (const auto& filter : options->filters.value()) {
      auto* filter_init = BluetoothLEScanFilterInit::Create();

      if (filter->name)
        filter_init->setName(filter->name);

      if (filter->name_prefix)
        filter_init->setNamePrefix(filter->name_prefix);

      if (filter->services && filter->services.has_value()) {
        HeapVector<blink::StringOrUnsignedLong> services;
        for (const auto& uuid : filter->services.value()) {
          blink::StringOrUnsignedLong uuid_string;
          uuid_string.SetString(uuid);
          services.push_back(uuid_string);
        }
        filter_init->setServices(services);
      }
      filters_.push_back(std::move(filter_init));
    }
  }
}

const HeapVector<Member<BluetoothLEScanFilterInit>>& BluetoothLEScan::filters()
    const {
  return filters_;
}

bool BluetoothLEScan::keepRepeatedDevices() const {
  return keep_repeated_devices_;
}
bool BluetoothLEScan::acceptAllAdvertisements() const {
  return accept_all_advertisements_;
}

bool BluetoothLEScan::active() const {
  return bluetooth_->IsScanActive(id_);
}

bool BluetoothLEScan::stop() {
  bluetooth_->CancelScan(id_);
  return true;
}

void BluetoothLEScan::Trace(blink::Visitor* visitor) {
  visitor->Trace(filters_);
  visitor->Trace(bluetooth_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
