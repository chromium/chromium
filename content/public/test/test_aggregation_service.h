// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_
#define CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

class GURL;

template <class T>
class scoped_refptr;

namespace base {
class Clock;
class Value;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace url {
class Origin;
}  // namespace url

namespace content {

// Interface for a test aggregation service which can be created without any
// dependencies. Supports configuring public keys at runtime.
class TestAggregationService {
 public:
  virtual ~TestAggregationService() = default;

  // Creates an instance of the service.
  static std::unique_ptr<TestAggregationService> Create(
      const base::Clock* clock);

  // Parses the keys for `origin` from `json_string`, and saves the set of keys
  // to storage. `callback` will be run once completed which takes a boolean
  // value indicating whether the keys were parsed successfully.
  virtual void SetPublicKeys(const url::Origin& origin,
                             const std::string& json_string,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Sets the provided URL loader factory. This will be called by the
  // aggregation service tool to inject a network::mojom::URLLoaderFactory to
  // send an aggregatable report over network.
  virtual void SetURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

  // Sends the aggregatable report to the specified reporting endpoint `url`.
  // `callback` will be run once completed which returns whether the report was
  // sent successfully.
  virtual void SendReport(const GURL& url,
                          const base::Value& contents,
                          base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_AGGREGATION_SERVICE_H_