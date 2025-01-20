// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
#define SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/shared_storage.mojom.h"

namespace network {

static constexpr char kSharedStoragePathPrefix[] = "/shared_storage";
static constexpr char kSharedStorageWritePathSuffix[] = "/write.html";
static constexpr char kSharedStorageTestPath[] = "/test";
static constexpr char kSharedStorageBypassPath[] = "/bypass";
static constexpr char kSharedStorageRedirectPath[] = "/redirect";
static constexpr char kSharedStorageResponseData[] =
    "shared storage response data";

std::string MakeSharedStorageTestPath();
std::string MakeSharedStorageBypassPath();
std::string MakeSharedStorageRedirectPrefix();

class SharedStorageRequestCount {
 public:
  static size_t Get();
  static size_t Increment();
  static void Reset();

 private:
  static size_t count_;
};

mojom::SharedStorageModifierMethodWithOptionsPtr MojomSetMethod(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    std::optional<std::string> with_lock = std::nullopt);

mojom::SharedStorageModifierMethodWithOptionsPtr MojomAppendMethod(
    const std::u16string& key,
    const std::u16string& value,
    std::optional<std::string> with_lock = std::nullopt);

mojom::SharedStorageModifierMethodWithOptionsPtr MojomDeleteMethod(
    const std::u16string& key,
    std::optional<std::string> with_lock = std::nullopt);

mojom::SharedStorageModifierMethodWithOptionsPtr MojomClearMethod(
    std::optional<std::string> with_lock = std::nullopt);

// Wraps `mojom::SharedStorageModifierMethodWithOptionsPtr` to use gmock
// matchers.
struct SharedStorageMethodWrapper {
  explicit SharedStorageMethodWrapper(
      mojom::SharedStorageModifierMethodWithOptionsPtr method_with_options);

  SharedStorageMethodWrapper(const SharedStorageMethodWrapper& other);
  SharedStorageMethodWrapper& operator=(
      const SharedStorageMethodWrapper& other);

  ~SharedStorageMethodWrapper();

  friend bool operator==(const SharedStorageMethodWrapper& a,
                         const SharedStorageMethodWrapper& b) = default;

  mojom::SharedStorageModifierMethodWithOptionsPtr method_with_options;
};

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageMethodWrapper& wrapper);

class SharedStorageResponse : public net::test_server::BasicHttpResponse {
 public:
  explicit SharedStorageResponse(std::string shared_storage_write);
  SharedStorageResponse(net::HttpStatusCode code, std::string new_location);
  SharedStorageResponse(std::string shared_storage_write,
                        net::HttpStatusCode code,
                        std::string new_location);

  SharedStorageResponse(const SharedStorageResponse&) = delete;
  SharedStorageResponse& operator=(const SharedStorageResponse&) = delete;

  ~SharedStorageResponse() override;

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override;

 private:
  std::optional<std::string> shared_storage_write_;
  net::HttpStatusCode code_ = net::HTTP_OK;
  std::optional<std::string> new_location_;
};

// Sends a response with the "Shared-Storage-Write" header, with value
// `shared_storage_write`, to any request whose path starts with the
// `kSharedStoragePathPrefix` prefix and which has the
// "Sec-Shared-Storage-Writable: ?1" request header or whose full path is
// `MakeSharedStorageBypassPath()`.
std::unique_ptr<net::test_server::HttpResponse>
HandleSharedStorageRequestSimple(std::string shared_storage_write,
                                 const net::test_server::HttpRequest& request);

// Sends a response with the "Shared-Storage-Write" header, with the next
// available value in `shared_storage_write_headers` as tracked by
// `SharedStorageRequestCount`, to any request whose path starts with the
// `kSharedStoragePathPrefix` prefix and ends with the
// `kSharedStorageWritePathSuffix` and which has the
// "Sec-Shared-Storage-Writable: ?1" request header, as long as
// `shared_storage_write_headers` has not yet been fully iterated through..
std::unique_ptr<net::test_server::HttpResponse>
HandleSharedStorageRequestMultiple(
    std::vector<std::string> shared_storage_write_headers,
    const net::test_server::HttpRequest& request);

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_TEST_UTILS_H_
