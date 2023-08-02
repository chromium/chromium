// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/error_page_response_provider.h"

// static
GURL ErrorPageResponseProvider::GetDnsFailureUrl() {
  return GURL("http://mock/bad");
}
