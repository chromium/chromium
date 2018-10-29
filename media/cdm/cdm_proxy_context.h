// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PROXY_CONTEXT_H_
#define MEDIA_CDM_CDM_PROXY_CONTEXT_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_proxy.h"

#if defined(OS_WIN)
#include <d3d11.h>
#endif

namespace media {

// An interface for accessing various CdmProxy data required for decrypting
// and/or decoding data.
class MEDIA_EXPORT CdmProxyContext {
 public:
#if defined(OS_WIN)
  struct D3D11DecryptContext {
    // Crypto session pointer for decryption.
    // The pointer is owned by the CdmContext implementation.
    ID3D11CryptoSession* crypto_session;

    // Opaque key blob for decrypting or decoding.
    // The pointer is owned by the CdmContext implementation.
    const void* key_blob;

    // The size of the blob.
    uint32_t key_blob_size;

    // GUID identifying the hardware key info.
    GUID key_info_guid;
  };

  // Returns D3D11DecryptContext on success. Returns nullopt otherwise. The
  // D3D11DecryptContext instance is only guaranteed to be valid before the
  // caller returns.
  // |key_type| is the requesting key type.
  // |key_id| is the key ID of the media to decrypt.
  virtual base::Optional<D3D11DecryptContext> GetD3D11DecryptContext(
      CdmProxy::KeyType key_type,
      const std::string& key_id) WARN_UNUSED_RESULT;
#endif  // defined(OS_WIN)

  // TODO(crbug.com/787657): There will be more methods for this class.

 protected:
  CdmProxyContext();
  virtual ~CdmProxyContext();

  DISALLOW_COPY_AND_ASSIGN(CdmProxyContext);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_PROXY_CONTEXT_H_
