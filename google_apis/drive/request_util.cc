// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_util.h"

#include <string>
#include "base/values.h"

namespace google_apis {
namespace util {

namespace {

// etag matching header.
const char kIfMatchHeaderPrefix[] = "If-Match: ";
const char kParentLinkKind[] = "drive#fileLink";

}  // namespace

const char kIfMatchAllHeader[] = "If-Match: *";
const char kContentTypeApplicationJson[] = "application/json";

std::string GenerateIfMatchHeader(const std::string& etag) {
  return etag.empty() ? kIfMatchAllHeader : (kIfMatchHeaderPrefix + etag);
}

base::Value::Dict CreateParentValue(const std::string& file_id) {
  base::Value::Dict parent;
  parent.Set("kind", kParentLinkKind);
  parent.Set("id", file_id);
  return parent;
}

}  // namespace util
}  // namespace google_apis
