// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/gstatic_json_fetcher.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {

namespace {

constexpr char kGstaticUrlPrefix[] = "https://www.gstatic.com/chromoting/";

base::Optional<base::Value> GetResponse(
    std::unique_ptr<net::URLFetcher> fetcher) {
  int response_code = fetcher->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    LOG(ERROR) << "Json fetch request failed with error code: "
               << response_code;
    return base::nullopt;
  }

  std::string response_string;
  if (!fetcher->GetResponseAsString(&response_string)) {
    LOG(ERROR) << "Failed to retrieve response data";
    return base::nullopt;
  }

  return base::JSONReader::Read(response_string);
}

}  // namespace

GstaticJsonFetcher::GstaticJsonFetcher(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner) {
  request_context_getter_ =
      new remoting::URLRequestContextGetter(network_task_runner);
}

GstaticJsonFetcher::~GstaticJsonFetcher() = default;

void GstaticJsonFetcher::FetchJsonFile(
    const std::string& relative_path,
    FetchJsonFileCallback done,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto fetcher =
      net::URLFetcher::Create(GetFullUrl(relative_path), net::URLFetcher::GET,
                              this, traffic_annotation);
  fetcher->SetRequestContext(request_context_getter_.get());
  auto* fetcher_ptr = fetcher.get();
  fetcher_callback_map_[std::move(fetcher)] = std::move(done);
  fetcher_ptr->Start();
}

// static
GURL GstaticJsonFetcher::GetFullUrl(const std::string& relative_path) {
  return GURL(kGstaticUrlPrefix + relative_path);
}

void GstaticJsonFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  auto find_fetcher = [source](const std::pair<std::unique_ptr<net::URLFetcher>,
                                               FetchJsonFileCallback>& pair) {
    return pair.first.get() == source;
  };
  auto it = std::find_if(fetcher_callback_map_.begin(),
                         fetcher_callback_map_.end(), find_fetcher);
  if (it == fetcher_callback_map_.end()) {
    LOG(DFATAL) << "Fetcher not found in the map";
    return;
  }
  // callback can potentially add new requests to the JSON fetcher, which makes
  // the iterator unstable, so we erase the iterator before running the
  // callback.
  auto callback =
      base::BindOnce(std::move(it->second), GetResponse(std::move(it->first)));
  fetcher_callback_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace remoting
