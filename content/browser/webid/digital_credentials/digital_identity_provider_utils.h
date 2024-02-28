// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_UTILS_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_UTILS_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content {

CONTENT_EXPORT std::unique_ptr<DigitalIdentityProvider>
CreateDigitalIdentityProvider();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_UTILS_H_
