// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/http_server_util.h"

#import <memory>

#import "base/path_service.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"

namespace web {
namespace test {

void SetUpSimpleHttpServer(const std::map<GURL, std::string>& responses) {
  SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));
}

void SetUpHttpServer(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());

  server.RemoveAllResponseProviders();
  server.AddResponseProvider(std::move(provider));
}

void AddResponseProvider(std::unique_ptr<web::ResponseProvider> provider) {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  DCHECK(server.IsRunning());
  server.AddResponseProvider(std::move(provider));
}

}  // namespace test
}  // namespace web
