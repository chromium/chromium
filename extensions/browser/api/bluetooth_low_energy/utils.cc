// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/utils.h"

#include <stddef.h>
#include <iterator>
#include <vector>

#include "base/check.h"

namespace extensions {
namespace api {
namespace bluetooth_low_energy {

namespace {

// Converts a list of CharacteristicProperty to a base::ListValue of strings.
base::ListValue CharacteristicPropertiesToList(
    const std::vector<CharacteristicProperty> properties) {
  base::ListValue property_list;
  for (const auto& property : properties) {
    property_list.Append(ToString(property));
  }
  return property_list;
}

}  // namespace

base::DictValue CharacteristicToValue(Characteristic& from) {
  // Copy the properties. Use Characteristic::ToValue to generate the result
  // dictionary without the properties, to prevent json_schema_compiler from
  // failing.
  std::vector<CharacteristicProperty> properties = std::move(from.properties);
  base::DictValue to = from.ToValue();
  to.Set("properties", CharacteristicPropertiesToList(properties));
  return to;
}

base::DictValue DescriptorToValue(Descriptor& from) {
  if (!from.characteristic) {
    return from.ToValue();
  }

  // Copy the characteristic properties and set them later manually.
  std::vector<CharacteristicProperty> properties =
      std::move(from.characteristic->properties);
  from.characteristic->properties.clear();
  base::DictValue to = from.ToValue();

  base::DictValue* chrc_value = to.FindDict("characteristic");
  DCHECK(chrc_value);
  chrc_value->Set("properties", CharacteristicPropertiesToList(properties));
  return to;
}

}  // namespace bluetooth_low_energy
}  // namespace api
}  // namespace extensions
