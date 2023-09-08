// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_MDOCS_MDOC_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_MDOCS_MDOC_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

#include <string>

using blink::mojom::WalletFieldRequirementPtr;

namespace content {

// Coordinates between the web and native apps such that the latter can share
// mdocs with the web API caller. The functions are platform agnostic and
// implementations are expected to be different across platforms like desktop
// and mobile.
class CONTENT_EXPORT MDocProvider {
 public:
  virtual ~MDocProvider();

  MDocProvider(const MDocProvider&) = delete;
  MDocProvider& operator=(const MDocProvider&) = delete;

  static std::unique_ptr<MDocProvider> Create();

  using MDocCallback = base::OnceCallback<void(std::string)>;
  virtual void RequestMDoc(WebContents* web_contents,
                           const url::Origin& origin,
                           const base::Value::Dict& request,
                           MDocCallback callback) = 0;

 protected:
  MDocProvider();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MDOCS_WEBID_MDOC_PROVIDER_H_
