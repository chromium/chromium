// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_CONTEXT_H_
#define MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_CONTEXT_H_

#include <memory>

namespace media {

class SysmemBufferStream;

// Interface for Fuchsia-specific extensions to the CdmContext interface.
class FuchsiaCdmContext {
 public:
  FuchsiaCdmContext() = default;

  // Creates FuchsiaSecureStreamDecryptor instance for the CDM context.
  virtual std::unique_ptr<SysmemBufferStream> CreateStreamDecryptor(
      bool secure_mode) = 0;

 protected:
  virtual ~FuchsiaCdmContext() = default;
};

}  // namespace media

#endif  // MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_CONTEXT_H_
