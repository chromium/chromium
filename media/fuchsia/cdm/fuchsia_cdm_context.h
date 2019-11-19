// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_CONTEXT_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_CONTEXT_H_

#include "media/fuchsia/cdm/fuchsia_stream_decryptor.h"

namespace media {

// Interface for Fuchsia-specific extensions to the CdmContext interface.
class FuchsiaCdmContext {
 public:
  FuchsiaCdmContext() = default;

  // Creates FuchsiaSecureStreamDecryptor instance for the CDM context.
  virtual std::unique_ptr<FuchsiaSecureStreamDecryptor> CreateVideoDecryptor(
      FuchsiaSecureStreamDecryptor::Client* client) = 0;

 protected:
  virtual ~FuchsiaCdmContext() = default;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_CONTEXT_H_
