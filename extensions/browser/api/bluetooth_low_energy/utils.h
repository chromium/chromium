// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_UTILS_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_UTILS_H_

#include "base/values.h"
#include "extensions/common/api/bluetooth_low_energy.h"

namespace extensions {
namespace api {
namespace bluetooth_low_energy {

// TODO(armansito): Remove these functions once the described bug is fixed.
// (See crbug.com/368368)

// Converts a Characteristic to a base::Value::Dict. This function is necessary
// as json_schema_compiler::util::AddItemToList has no template specialization
// for user defined enums, which get treated as integers. This is because
// Characteristic contains a list of enum CharacteristicProperty.
base::Value::Dict CharacteristicToValue(Characteristic& from);

// Converts a Descriptor to a base::Value. This function is necessary as a
// Descriptor embeds a Characteristic and that needs special handling as
// described above.
base::Value::Dict DescriptorToValue(Descriptor& from);

}  // namespace bluetooth_low_energy
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_UTILS_H_
