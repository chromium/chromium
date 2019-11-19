// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_
#define REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "remoting/client/notification/json_fetcher.h"
#include "url/gurl.h"

namespace remoting {

class URLRequestContextGetter;

// A JsonFetcher implementation that actually talks to the gstatic server to
// get back the JSON files.
class GstaticJsonFetcher final : public JsonFetcher,
                                 public net::URLFetcherDelegate {
 public:
  explicit GstaticJsonFetcher(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);
  ~GstaticJsonFetcher() override;

  // JsonFetcher implementation.
  void FetchJsonFile(
      const std::string& relative_path,
      FetchJsonFileCallback done,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  friend class GstaticJsonFetcherTest;

  static GURL GetFullUrl(const std::string& relative_path);

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  base::flat_map<std::unique_ptr<net::URLFetcher>, FetchJsonFileCallback>
      fetcher_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(GstaticJsonFetcher);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_GSTATIC_JSON_FETCHER_H_