// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"

namespace blink {

class BluetoothServiceDataMapIterationSource final
    : public PairIterable<String, Member<DOMDataView>>::IterationSource {
 public:
  BluetoothServiceDataMapIterationSource(const BluetoothServiceDataMap& map)
      : map_(map), iterator_(map_->Map().begin()) {}

  bool Next(ScriptState* script_state,
            String& map_key,
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
    PairIterable<String, Member<DOMDataView>>::IterationSource::Trace(visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const BluetoothServiceDataMap> map_;
  BluetoothServiceDataMap::MapType::const_iterator iterator_;
};

BluetoothServiceDataMap::BluetoothServiceDataMap(
    const BluetoothServiceDataMap::MapType& map)
    : parameter_map_(map) {}

BluetoothServiceDataMap::~BluetoothServiceDataMap() {}

PairIterable<String, Member<DOMDataView>>::IterationSource*
BluetoothServiceDataMap::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<BluetoothServiceDataMapIterationSource>(*this);
}

bool BluetoothServiceDataMap::GetMapEntry(ScriptState*,
                                          const String& key,
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
