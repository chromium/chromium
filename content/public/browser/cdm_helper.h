// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CDM_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_CDM_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

/**
 * A helper class to interact with CDM. Used by GapisService component to
 * fetch app tokens.
 */
class CONTENT_EXPORT CdmHelper {
 public:
  static std::unique_ptr<CdmHelper> CreateInstance();

  // The result of the SignChallenge operation.
  enum class SignChallengeResult {
    kSuccess,
    kCdmError,
  };

  // The result of the Initialize operation.
  enum class InitializeResult {
    kSuccess,
    kInitializeError,
  };

  virtual ~CdmHelper();

  // Initializes the CDM helper object with the given server certificate. The
  // `init_callback` is called with the initialization result.
  virtual void Initialize(
      const std::string& server_ceritificate,
      const std::string& key_system,
      base::OnceCallback<void(InitializeResult)> init_callback) = 0;

  // Signs the given `challenge` using the CDM. The `callback` is called with
  // the signature and a status code.
  // TODO(crbug.com/485217840): this method can only be called once at a time,
  // it should be fixed in the future.
  virtual void SignChallenge(
      const std::string& challenge,
      base::OnceCallback<void(const std::string&, SignChallengeResult)>
          callback) = 0;

 protected:
  CdmHelper();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CDM_HELPER_H_
