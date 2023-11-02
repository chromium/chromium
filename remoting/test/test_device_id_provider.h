// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_DEVICE_ID_PROVIDER_H_
#define REMOTING_TEST_TEST_DEVICE_ID_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "remoting/signaling/ftl_device_id_provider.h"

namespace remoting {
namespace test {

// The FtlDeviceIdProvider implementation that generates device ID then store
// and reuse it from |token_storage|.
class TestDeviceIdProvider final : public FtlDeviceIdProvider {
 public:
  class TokenStorage {
   public:
    TokenStorage() = default;
    virtual ~TokenStorage() = default;

    virtual std::string FetchDeviceId() = 0;
    virtual bool StoreDeviceId(const std::string& device_id) = 0;
  };

  explicit TestDeviceIdProvider(TokenStorage* token_storage);

  TestDeviceIdProvider(const TestDeviceIdProvider&) = delete;
  TestDeviceIdProvider& operator=(const TestDeviceIdProvider&) = delete;

  ~TestDeviceIdProvider() override;

  // FtlDeviceIdProvider implementations.
  ftl::DeviceId GetDeviceId() override;

 private:
  raw_ptr<TokenStorage> token_storage_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_TEST_DEVICE_ID_PROVIDER_H_
