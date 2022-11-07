// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_
#define REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/client/notification/json_fetcher.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace remoting {

// A JsonFetcher implementation that actually talks to the gstatic server to
// get back the JSON files.
class GstaticJsonFetcher final : public JsonFetcher {
 public:
  explicit GstaticJsonFetcher(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  GstaticJsonFetcher(const GstaticJsonFetcher&) = delete;
  GstaticJsonFetcher& operator=(const GstaticJsonFetcher&) = delete;

  ~GstaticJsonFetcher() override;

  // JsonFetcher implementation.
  void FetchJsonFile(
      const std::string& relative_path,
      FetchJsonFileCallback done,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  friend class GstaticJsonFetcherTest;

  static GURL GetFullUrl(const std::string& relative_path);

  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> body);

  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner_;

  base::flat_map<std::unique_ptr<network::SimpleURLLoader>,
                 FetchJsonFileCallback>
      loader_callback_map_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_
