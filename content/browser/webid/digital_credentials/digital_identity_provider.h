// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#include <string>

namespace content {

// Coordinates between the web and native apps such that the latter can share
// vcs with the web API caller. The functions are platform agnostic and
// implementations are expected to be different across platforms like desktop
// and mobile.
class CONTENT_EXPORT DigitalIdentityProvider {
 public:
  virtual ~DigitalIdentityProvider();

  DigitalIdentityProvider(const DigitalIdentityProvider&) = delete;
  DigitalIdentityProvider& operator=(const DigitalIdentityProvider&) = delete;

  static std::unique_ptr<DigitalIdentityProvider> Create();

  using DigitalIdentityCallback = base::OnceCallback<void(const std::string&)>;
  virtual void Request(WebContents* web_contents,
                       const url::Origin& origin,
                       const base::Value::Dict& request,
                       DigitalIdentityCallback callback) = 0;

 protected:
  DigitalIdentityProvider();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_H_
