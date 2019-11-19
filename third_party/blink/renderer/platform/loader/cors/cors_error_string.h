// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_ERROR_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_ERROR_STRING_H_

#include "base/macros.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/mojom/cors.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class KURL;
class SecurityOrigin;
enum class ResourceType : uint8_t;

// CORS error strings related utility functions.
namespace cors {

// Stringify CorsError mainly for inspector messages. Generated string should
// not be exposed to JavaScript for security reasons.
PLATFORM_EXPORT String GetErrorString(const network::CorsErrorStatus& status,
                                      const KURL& initial_request_url,
                                      const KURL& last_request_url,
                                      const SecurityOrigin& origin,
                                      ResourceType resource_type,
                                      const AtomicString& initiator_name);

}  // namespace cors

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_CORS_CORS_ERROR_STRING_H_
