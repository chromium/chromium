// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"

namespace blink {

class BluetoothManufacturerDataMapIterationSource final
    : public PairSyncIterable<BluetoothManufacturerDataMap>::IterationSource {
 public:
  explicit BluetoothManufacturerDataMapIterationSource(
      const BluetoothManufacturerDataMap& map)
      : map_(map), iterator_(map_->Map().begin()) {}

  bool FetchNextItem(ScriptState* script_state,
                     uint16_t& map_key,
                     NotShared<DOMDataView>& map_value,
                     ExceptionState&) override {
    if (iterator_ == map_->Map().end())
      return false;
    map_key = iterator_->key->id;
    map_value = NotShared<DOMDataView>(
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(iterator_->value));
    ++iterator_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    PairSyncIterable<BluetoothManufacturerDataMap>::IterationSource::Trace(
        visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const BluetoothManufacturerDataMap> map_;
  BluetoothManufacturerDataMap::MapType::const_iterator iterator_;
};

BluetoothManufacturerDataMap::BluetoothManufacturerDataMap(
    const BluetoothManufacturerDataMap::MapType& map) {
  for (const auto& entry : map) {
    parameter_map_.insert(entry.key.Clone(), entry.value);
  }
}

BluetoothManufacturerDataMap::~BluetoothManufacturerDataMap() {}

PairSyncIterable<BluetoothManufacturerDataMap>::IterationSource*
BluetoothManufacturerDataMap::CreateIterationSource(ScriptState*,
                                                    ExceptionState&) {
  return MakeGarbageCollected<BluetoothManufacturerDataMapIterationSource>(
      *this);
}

bool BluetoothManufacturerDataMap::GetMapEntry(ScriptState*,
                                               const uint16_t& key,
                                               NotShared<DOMDataView>& value,
                                               ExceptionState&) {
  mojom::blink::WebBluetoothCompanyPtr company =
      mojom::blink::WebBluetoothCompany::New(key);
  auto it = parameter_map_.find(company);
  if (it == parameter_map_.end())
    return false;

  DOMDataView* dom_data_view =
      BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(it->value);

  value = NotShared<DOMDataView>(dom_data_view);
  return true;
}

}  // namespace blink
