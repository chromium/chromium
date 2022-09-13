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

std::unique_ptr<base::DictionaryValue> CreateParentValue(
    const std::string& file_id) {
  std::unique_ptr<base::DictionaryValue> parent(new base::DictionaryValue);
  parent->SetString("kind", kParentLinkKind);
  parent->SetString("id", file_id);
  return parent;
}

}  // namespace util
}  // namespace google_apis
