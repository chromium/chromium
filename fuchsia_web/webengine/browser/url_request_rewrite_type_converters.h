// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_

#include <fuchsia/web/cpp/fidl.h>

#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// This conversion is done with a TypeCoverter rather than a typemap because
// it is only done one way, from the FIDL type to the Mojo type. This conversion
// is only done once, in the browser process. These rules are validated after
// they have been converted into Mojo.
// In WebEngine, we have a one-way flow from the untrusted embedder into the
// browser process, via a FIDL API. From there, the rules are converted into
// Mojo and then validated before being sent to renderer processes. No further
// conversion is performed, the Mojo types are used as is to apply the rewrites
// on URL requests.
template <>
struct WEB_ENGINE_EXPORT
    TypeConverter<url_rewrite::mojom::UrlRequestRewriteRulesPtr,
                  std::vector<fuchsia::web::UrlRequestRewriteRule>> {
  static url_rewrite::mojom::UrlRequestRewriteRulesPtr Convert(
      const std::vector<fuchsia::web::UrlRequestRewriteRule>& input);
};

}  // namespace mojo

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
