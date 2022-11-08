// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_SPAWNER_REQUEST_H_
#define NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_SPAWNER_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {

class ScopedPortException;

// RemoteTestServerSpawnerRequest is used by RemoteTestServer to send a request
// to the test server spawner.
class RemoteTestServerSpawnerRequest {
 public:
  // Queries the specified URL. If |post_data| is empty then a GET request is
  // sent. Otherwise |post_data| must be a json blob which is sent as a POST
  // request body.
  RemoteTestServerSpawnerRequest(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const GURL& url,
      const std::string& post_data);

  RemoteTestServerSpawnerRequest(const RemoteTestServerSpawnerRequest&) =
      delete;
  RemoteTestServerSpawnerRequest& operator=(
      const RemoteTestServerSpawnerRequest&) = delete;

  ~RemoteTestServerSpawnerRequest();

  // Blocks until request is finished. If |response| isn't nullptr then server
  // response is copied to *response. Returns true if the request was completed
  // successfully.
  [[nodiscard]] bool WaitForCompletion(std::string* response);

 private:
  class Core;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Core runs on |io_task_runner_|. It's responsible for sending the request
  // and reading the response.
  std::unique_ptr<Core> core_;

  // Helper to add spawner port to the list of the globally explicitly allowed
  // ports. It needs to be here instead of in Core because ScopedPortException
  // is not thread-safe.
  std::unique_ptr<ScopedPortException> allowed_port_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_REMOTE_TEST_SERVER_SPAWNER_REQUEST_H_
