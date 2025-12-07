// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_CDM_CONTEXT_FOR_OOPVD_TYPES_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_CDM_CONTEXT_FOR_OOPVD_TYPES_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/video_decoder.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::CdmContextEvent, ::media::CdmContext::Event> {
  static media::mojom::CdmContextEvent ToMojom(
      ::media::CdmContext::Event input) {
    switch (input) {
      case ::media::CdmContext::Event::kHasAdditionalUsableKey:
        return media::mojom::CdmContextEvent::kHasAdditionalUsableKey;
      case ::media::CdmContext::Event::kHardwareContextReset:
        return media::mojom::CdmContextEvent::kHardwareContextReset;
    }

    NOTREACHED();
  }

  static bool FromMojom(media::mojom::CdmContextEvent input,
                        ::media::CdmContext::Event* output) {
    switch (input) {
      case media::mojom::CdmContextEvent::kHasAdditionalUsableKey:
        *output = ::media::CdmContext::Event::kHasAdditionalUsableKey;
        return true;
      case media::mojom::CdmContextEvent::kHardwareContextReset:
        *output = ::media::CdmContext::Event::kHardwareContextReset;
        return true;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::DecryptStatus, ::media::Decryptor::Status> {
  static media::mojom::DecryptStatus ToMojom(::media::Decryptor::Status input) {
    switch (input) {
      case ::media::Decryptor::Status::kSuccess:
        return media::mojom::DecryptStatus::kSuccess;
      case ::media::Decryptor::Status::kNoKey:
        return media::mojom::DecryptStatus::kNoKey;
      case ::media::Decryptor::Status::kNeedMoreData:
        return media::mojom::DecryptStatus::kFailure;
      case ::media::Decryptor::Status::kError:
        return media::mojom::DecryptStatus::kFailure;
    }

    NOTREACHED();
  }

  static bool FromMojom(media::mojom::DecryptStatus input,
                        ::media::Decryptor::Status* output) {
    switch (input) {
      case media::mojom::DecryptStatus::kSuccess:
        *output = ::media::Decryptor::Status::kSuccess;
        return true;
      case media::mojom::DecryptStatus::kNoKey:
        *output = ::media::Decryptor::Status::kNoKey;
        return true;
      case media::mojom::DecryptStatus::kFailure:
        *output = ::media::Decryptor::Status::kError;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_CDM_CONTEXT_FOR_OOPVD_TYPES_MOJOM_TRAITS_H_
