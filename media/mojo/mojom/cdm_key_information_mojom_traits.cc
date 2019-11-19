// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/cdm_key_information_mojom_traits.h"

#include "base/logging.h"

namespace mojo {

using MojomKeyStatus = media::mojom::CdmKeyStatus;
using NativeKeyStatus = media::CdmKeyInformation::KeyStatus;

// static
MojomKeyStatus EnumTraits<MojomKeyStatus, NativeKeyStatus>::ToMojom(
    NativeKeyStatus error) {
  switch (error) {
    case NativeKeyStatus::USABLE:
      return MojomKeyStatus::USABLE;
    case NativeKeyStatus::INTERNAL_ERROR:
      return MojomKeyStatus::INTERNAL_ERROR;
    case NativeKeyStatus::EXPIRED:
      return MojomKeyStatus::EXPIRED;
    case NativeKeyStatus::OUTPUT_RESTRICTED:
      return MojomKeyStatus::OUTPUT_RESTRICTED;
    case NativeKeyStatus::OUTPUT_DOWNSCALED:
      return MojomKeyStatus::OUTPUT_DOWNSCALED;
    case NativeKeyStatus::KEY_STATUS_PENDING:
      return MojomKeyStatus::KEY_STATUS_PENDING;
    case NativeKeyStatus::RELEASED:
      return MojomKeyStatus::RELEASED;
  }
  NOTREACHED();
  return MojomKeyStatus::INTERNAL_ERROR;
}

// static
bool EnumTraits<MojomKeyStatus, NativeKeyStatus>::FromMojom(
    MojomKeyStatus error,
    NativeKeyStatus* out) {
  switch (error) {
    case MojomKeyStatus::USABLE:
      *out = NativeKeyStatus::USABLE;
      return true;
    case MojomKeyStatus::INTERNAL_ERROR:
      *out = NativeKeyStatus::INTERNAL_ERROR;
      return true;
    case MojomKeyStatus::EXPIRED:
      *out = NativeKeyStatus::EXPIRED;
      return true;
    case MojomKeyStatus::OUTPUT_RESTRICTED:
      *out = NativeKeyStatus::OUTPUT_RESTRICTED;
      return true;
    case MojomKeyStatus::OUTPUT_DOWNSCALED:
      *out = NativeKeyStatus::OUTPUT_DOWNSCALED;
      return true;
    case MojomKeyStatus::KEY_STATUS_PENDING:
      *out = NativeKeyStatus::KEY_STATUS_PENDING;
      return true;
    case MojomKeyStatus::RELEASED:
      *out = NativeKeyStatus::RELEASED;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::CdmKeyInformationDataView,
                  std::unique_ptr<media::CdmKeyInformation>>::
    Read(media::mojom::CdmKeyInformationDataView input,
         std::unique_ptr<media::CdmKeyInformation>* output) {
  mojo::ArrayDataView<uint8_t> key_id;
  input.GetKeyIdDataView(&key_id);

  NativeKeyStatus status;
  if (!input.ReadStatus(&status))
    return false;

  *output = std::make_unique<media::CdmKeyInformation>(
      key_id.data(), key_id.size(), status, input.system_code());
  return true;
}

}  // namespace mojo