// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"

namespace blink {

class BluetoothManufacturerDataMapIterationSource final
    : public PairIterable<uint16_t, Member<DOMDataView>>::IterationSource {
 public:
  explicit BluetoothManufacturerDataMapIterationSource(
      const BluetoothManufacturerDataMap& map)
      : map_(map), iterator_(map_->Map().begin()) {}

  bool Next(ScriptState* script_state,
            uint16_t& map_key,
            Member<DOMDataView>& map_value,
            ExceptionState&) override {
    if (iterator_ == map_->Map().end())
      return false;
    map_key = iterator_->key;
    map_value =
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(iterator_->value);
    ++iterator_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    PairIterable<uint16_t, Member<DOMDataView>>::IterationSource::Trace(
        visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const BluetoothManufacturerDataMap> map_;
  BluetoothManufacturerDataMap::MapType::const_iterator iterator_;
};

BluetoothManufacturerDataMap::BluetoothManufacturerDataMap(
    const BluetoothManufacturerDataMap::MapType& map)
    : parameter_map_(map) {}

BluetoothManufacturerDataMap::~BluetoothManufacturerDataMap() {}

PairIterable<uint16_t, Member<DOMDataView>>::IterationSource*
BluetoothManufacturerDataMap::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<BluetoothManufacturerDataMapIterationSource>(
      *this);
}

bool BluetoothManufacturerDataMap::GetMapEntry(ScriptState*,
                                               const uint16_t& key,
                                               Member<DOMDataView>& value,
                                               ExceptionState&) {
  auto it = parameter_map_.find(key);
  if (it == parameter_map_.end())
    return false;

  DOMDataView* dom_data_view =
      BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(it->value);

  value = dom_data_view;
  return true;
}

}  // namespace blink
