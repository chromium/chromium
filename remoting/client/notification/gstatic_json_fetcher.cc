// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/gstatic_json_fetcher.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "net/http/http_status_code.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace remoting {

namespace {

constexpr char kGstaticUrlPrefix[] = "https://www.gstatic.com/chromoting/";

std::optional<base::Value> GetResponse(std::unique_ptr<std::string> body) {
  if (!body)
    return std::nullopt;

  return base::JSONReader::Read(*body);
}

}  // namespace

GstaticJsonFetcher::GstaticJsonFetcher(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : url_loader_factory_owner_(
          base::MakeRefCounted<remoting::URLRequestContextGetter>(
              network_task_runner)) {}

GstaticJsonFetcher::~GstaticJsonFetcher() = default;

void GstaticJsonFetcher::FetchJsonFile(
    const std::string& relative_path,
    FetchJsonFileCallback done,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GetFullUrl(relative_path);

  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  auto* loader_ptr = loader.get();
  loader_callback_map_[std::move(loader)] = std::move(done);
  loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_owner_.GetURLLoaderFactory().get(),
      base::BindOnce(&GstaticJsonFetcher::OnURLLoadComplete,
                     base::Unretained(this), loader_ptr));
}

// static
GURL GstaticJsonFetcher::GetFullUrl(const std::string& relative_path) {
  return GURL(kGstaticUrlPrefix + relative_path);
}

void GstaticJsonFetcher::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  auto it =
      base::ranges::find(loader_callback_map_, source,
                         [](const auto& pair) { return pair.first.get(); });
  if (it == loader_callback_map_.end()) {
    LOG(DFATAL) << "Fetcher not found in the map";
    return;
  }
  // callback can potentially add new requests to the JSON fetcher, which makes
  // the iterator unstable, so we erase the iterator before running the
  // callback.
  auto callback =
      base::BindOnce(std::move(it->second), GetResponse(std::move(body)));
  loader_callback_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace remoting
