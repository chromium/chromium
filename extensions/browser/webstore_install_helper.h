// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WEBSTORE_INSTALL_HELPER_H_
#define EXTENSIONS_BROWSER_WEBSTORE_INSTALL_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace network {
class SharedURLLoaderFactory;
}

namespace extensions {

enum class WebstoreInstallHelperResultCode {
  kUnknownError,
  kIconError,
  kManifestError,
};

struct WebstoreParsedData {
  SkBitmap icon;
  base::DictValue manifest;
};

struct WebstoreParseError {
  WebstoreInstallHelperResultCode error_code =
      WebstoreInstallHelperResultCode::kUnknownError;
  std::string error_message;
};

using WebstoreParseResult =
    base::expected<WebstoreParsedData, WebstoreParseError>;

using WebstoreParseCallback = base::OnceCallback<void(WebstoreParseResult)>;

// Parses the manifest and fetches the icon. The callback is guaranteed to be
// invoked asynchronously.
void ParseWebstoreData(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const std::string& id,
    const std::string& manifest,
    const GURL& icon_url,
    WebstoreParseCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_WEBSTORE_INSTALL_HELPER_H_
