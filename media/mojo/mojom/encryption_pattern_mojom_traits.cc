// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/encryption_pattern_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::EncryptionPatternDataView,
                  media::EncryptionPattern>::
    Read(media::mojom::EncryptionPatternDataView input,
         media::EncryptionPattern* output) {
  *output = media::EncryptionPattern(input.crypt_byte_block(),
                                     input.skip_byte_block());
  return true;
}

}  // namespace mojo
