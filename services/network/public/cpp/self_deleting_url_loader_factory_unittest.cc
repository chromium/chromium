// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class MockSelfDeletingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  MockSelfDeletingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      base::OnceClosure callback)
      : network::SelfDeletingURLLoaderFactory(std::move(pending_receiver)),
        callback_(std::move(callback)) {}

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {}

  ~MockSelfDeletingURLLoaderFactory() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
};

class SelfDeletingURLLoaderFactoryTest : public testing::Test {
 protected:
  SelfDeletingURLLoaderFactoryTest() = default;
  ~SelfDeletingURLLoaderFactoryTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

TEST_F(SelfDeletingURLLoaderFactoryTest, InitializeAndReset) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  base::RunLoop run_loop;

  new MockSelfDeletingURLLoaderFactory(
      pending_remote.InitWithNewPipeAndPassReceiver(), run_loop.QuitClosure());

  pending_remote.reset();
  run_loop.Run();
}

}  // namespace
}  // namespace network
