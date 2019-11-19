// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_

#include "media/base/encryption_pattern.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::EncryptionPatternDataView,
                    media::EncryptionPattern> {
  static uint32_t crypt_byte_block(const media::EncryptionPattern& input) {
    return input.crypt_byte_block();
  }

  static uint32_t skip_byte_block(const media::EncryptionPattern& input) {
    return input.skip_byte_block();
  }

  static bool Read(media::mojom::EncryptionPatternDataView input,
                   media::EncryptionPattern* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_ENCRYPTION_PATTERN_MOJOM_TRAITS_H_
