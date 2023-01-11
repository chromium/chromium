// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_CONSENT_PROVIDER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {

class Extension;

class ConsentProvider {
 public:
  enum Consent { CONSENT_GRANTED, CONSENT_REJECTED, CONSENT_IMPOSSIBLE };
  using ConsentCallback = base::OnceCallback<void(Consent)>;

  ConsentProvider() = default;
  ConsentProvider(const ConsentProvider&) = delete;
  ConsentProvider& operator=(const ConsentProvider&) = delete;
  virtual ~ConsentProvider() = default;

  // Requests consent for granting |writable| permissions to a volume with
  // |volume_id| and |volume_label| by |extension|, which is assumed to be
  // grantable (i.e., passes IsGrantable()).
  virtual void RequestConsent(content::RenderFrameHost* host,
                              const Extension& extension,
                              const std::string& volume_id,
                              const std::string& volume_label,
                              bool writable,
                              ConsentCallback callback) = 0;

  // Checks whether the |extension| can be granted access.
  virtual bool IsGrantable(const Extension& extension) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
