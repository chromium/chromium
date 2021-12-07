// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

namespace {

bool PathContainsDisallowedCharacter(const GURL& url) {
  std::string path = url.path();
  DCHECK(base::IsStringUTF8(path));

  // We should avoid these escaped characters in the path component because
  // these can be handled differently depending on server implementation.
  if (path.find("%2f") != std::string::npos ||
      path.find("%2F") != std::string::npos) {
    return true;
  }
  if (path.find("%5c") != std::string::npos ||
      path.find("%5C") != std::string::npos) {
    return true;
  }
  return false;
}

}  // namespace

// static
bool ServiceWorkerUtils::ContainsDisallowedCharacter(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  if (PathContainsDisallowedCharacter(scope) ||
      PathContainsDisallowedCharacter(script_url)) {
    *error_message = "The provided scope ('";
    error_message->append(scope.spec());
    error_message->append("') or scriptURL ('");
    error_message->append(script_url.spec());
    error_message->append("') includes a disallowed escape character.");
    return true;
  }
  return false;
}

}  // namespace content
