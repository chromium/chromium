// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_ACTIVATION_SEQUENCE_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_ACTIVATION_SEQUENCE_MOJOM_TRAITS_H_

#include "extensions/common/activation_sequence.h"
#include "extensions/common/mojom/activation_sequence.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::ActivationSequenceDataView,
                    extensions::ActivationSequence> {
  static int value(const extensions::ActivationSequence& activation_sequence) {
    return static_cast<int>(activation_sequence);
  }

  static bool Read(extensions::mojom::ActivationSequenceDataView data,
                   extensions::ActivationSequence* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_ACTIVATION_SEQUENCE_MOJOM_TRAITS_H_
