// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/cdm_info.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class CONTENT_EXPORT KeySystemSupportImpl final
    : public media::mojom::KeySystemSupport {
 public:
  KeySystemSupportImpl();
  ~KeySystemSupportImpl() final;

  // Create a KeySystemSupportImpl object and bind it to |receiver|.
  static void Create(
      mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver);

  // Returns CdmInfo registered for |key_system|. Returns null if no CdmInfo is
  // registered for |key_system|, or if the CdmInfo registered is invalid.
  static std::unique_ptr<CdmInfo> GetCdmInfoForKeySystem(
      const std::string& key_system);

  // media::mojom::KeySystemSupport implementation.
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeySystemSupportImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_IMPL_H_
