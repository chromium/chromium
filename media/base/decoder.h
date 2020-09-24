// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_H_
#define MEDIA_BASE_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media {

class MEDIA_EXPORT Decoder {
 public:
  virtual ~Decoder();

  // Returns true if the implementation is expected to be implemented by the
  // platform. The value should be available immediately after construction and
  // should not change within the lifetime of a decoder instance.
  virtual bool IsPlatformDecoder() const;

  // Returns true if the implementation supports decoding configs with
  // encryption.
  // TODO(crbug.com/1099488): Sometimes it's not possible to give a definitive
  // yes or no answer unless more context is given. While this doesn't pose any
  // problems, it does allow incompatible decoders to pass the filtering step in
  // |DecoderSelector| potentially slowing down the selection process.
  virtual bool SupportsDecryption() const;

  // Returns the name of the decoder for logging and decoder selection purposes.
  // This name should be available immediately after construction, and should
  // also be stable in the sense that the name does not change across multiple
  // constructions.
  virtual std::string GetDisplayName() const = 0;

 protected:
  Decoder();
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_H_
