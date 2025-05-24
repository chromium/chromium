// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_VALIDATION_UTILS_H_
#define MEDIA_MOJO_COMMON_VALIDATION_UTILS_H_

#include "media/mojo/mojom/video_decoder.mojom.h"

// TODO(crbug.com/347331029): add unit tests.

namespace media {

// TODO(crbug.com/390706725): move this validation to media_type_converters.cc
// when we've verified that users of those converters can deal with a null
// return type. Eventually migrate to typemaps.
// TODO(crbug.com/40468949): migrate to typemaps.
std::unique_ptr<media::DecryptConfig> ValidateAndConvertMojoDecryptConfig(
    media::mojom::DecryptConfigPtr decrypt_config);

// TODO(crbug.com/390706725): move this validation to media_type_converters.cc
// when we've verified that users of those converters are not affected by the
// additional validation. Eventually migrate to typemaps.
// TODO(crbug.com/40468949): migrate to typemaps.
std::unique_ptr<media::DecoderBufferSideData>
ValidateAndConvertMojoDecoderBufferSideData(
    media::mojom::DecoderBufferSideDataPtr side_data);

// TODO(crbug.com/390706725): move this validation to media_type_converters.cc
// when we've verified that users of those converters can deal with a null
// return type. Eventually migrate to typemaps.
// TODO(crbug.com/40468949): migrate to typemaps.
scoped_refptr<media::DecoderBuffer> ValidateAndConvertMojoDecoderBuffer(
    media::mojom::DecoderBufferPtr decoder_buffer);

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_VALIDATION_UTILS_H_
