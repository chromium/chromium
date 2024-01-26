// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_

#include <memory>

#import "base/memory/raw_ptr.h"

namespace web {

class WebClient;

// Helper class to register a WebClient during unit testing.
class ScopedTestingWebClient {
 public:
  explicit ScopedTestingWebClient(std::unique_ptr<WebClient> web_client);
  ~ScopedTestingWebClient();

  WebClient* Get();

 private:
  std::unique_ptr<WebClient> web_client_;
  raw_ptr<WebClient> original_web_client_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_
