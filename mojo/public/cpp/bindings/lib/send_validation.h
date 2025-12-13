// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_H_

#include "mojo/public/cpp/bindings/lib/has_send_validation_helper.h"
#include "mojo/public/cpp/bindings/lib/send_validation_type.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo::internal {

// Fallback to regular Serializer if SendValidationSerializer does not exist.
template <typename MojomType,
          SendValidation send_validation,
          typename MaybeConstUserType>
using SelectSerializer = std::conditional_t<
    HasSendValidationSerializer<MojomType,
                                MaybeConstUserType,
                                send_validation>::value,
    SendValidationSerializer<MojomType, MaybeConstUserType, send_validation>,
    Serializer<MojomType, MaybeConstUserType>>;

template <typename MojomType,
          SendValidation send_validation,
          typename InputUserType,
          typename... Args>
void Serialize(InputUserType&& input, Args&&... args) {
  if constexpr (IsStdOptional<InputUserType>::value) {
    if (!input) {
      return;
    }
    // Deduce the type of the dereferenced input to validate
    // if a send_validation Serialize is defined
    using DereferencedType = decltype(*input);
    if constexpr (HasSendValidationSerialize_v<MojomType, DereferencedType>) {
      Serialize<MojomType, send_validation>(*input,
                                            std::forward<Args>(args)...);
    } else {
      Serialize<MojomType>(*input, std::forward<Args>(args)...);
    }
  } else if constexpr (IsOptionalAsPointer<InputUserType>::value) {
    if (!input.has_value()) {
      return;
    }

    // Deduce the type of the dereferenced input to validate
    // if a send_validation Serialize is defined
    using DereferencedType = decltype(input.value());
    if constexpr (HasSendValidationSerialize_v<MojomType, DereferencedType>) {
      Serialize<MojomType, send_validation>(input.value(),
                                            std::forward<Args>(args)...);
    } else {
      Serialize<MojomType>(input.value(), std::forward<Args>(args)...);
    }
  } else {
    SelectSerializer<MojomType, send_validation,
                     std::remove_reference_t<InputUserType>>::
        Serialize(std::forward<InputUserType>(input),
                  std::forward<Args>(args)...);
  }
}

// This template is used to deduce the value of InputUserType when
// SendValidation is explicitly passed in.
template <typename MojomType, SendValidation send_validation, typename... Args>
void Serialize(Args&&... args) {
  Serialize<MojomType, send_validation>(std::forward<Args>(args)...);
}

// This is used for Runtime selection of Send Validation.
template <typename MojomType, typename... Args>
void SerializeWithSendValidation(SendValidation send_validation,
                                 Args&&... args) {
  switch (send_validation) {
    case SendValidation::kFatal:
      Serialize<MojomType, SendValidation::kFatal>(std::forward<Args>(args)...);
      break;
    case SendValidation::kWarning:
      Serialize<MojomType, SendValidation::kWarning>(
          std::forward<Args>(args)...);
      break;
  }
}

}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SEND_VALIDATION_H_
