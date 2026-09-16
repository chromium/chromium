// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/disk_cache/mojo_shared_http_cache_client_remote.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TestSharedHttpCacheClientFactory
    : public mojom::SharedHttpCacheClientFactory {
 public:
  void CreateClient(sqlite_vfs::PendingFileSet pending_file_set,
                    mojo::PendingReceiver<mojom::SharedHttpCacheClient>
                        client_receiver) override {
    create_client_future_.SetValue(std::move(client_receiver));
  }

  mojo::PendingReceiver<mojom::SharedHttpCacheClient> TakeClientReceiver() {
    return create_client_future_.Take();
  }

 private:
  base::test::TestFuture<mojo::PendingReceiver<mojom::SharedHttpCacheClient>>
      create_client_future_;
};

class MojoSharedHttpCacheClientRemoteTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  sqlite_vfs::PendingFileSet CreateDummyPendingFileSet() {
    base::FilePath db_path = temp_dir_.GetPath().AppendASCII("db");
    CHECK(base::WriteFile(db_path, ""));
    sqlite_vfs::PendingFileSet file_set;
    file_set.db_file =
        base::File(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    file_set.shared_lock = base::UnsafeSharedMemoryRegion::Create(64);
    return file_set;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

// Tests that if `factory_remote_` is already disconnected before
// `SetDisconnectHandler()` is called, `OnDisconnected()` is posted
// asynchronously rather than running synchronously.
TEST_F(MojoSharedHttpCacheClientRemoteTest,
       DisconnectedBeforeSetDisconnectHandler) {
  mojo::PendingRemote<mojom::SharedHttpCacheClientFactory> factory_remote;
  mojo::PendingReceiver<mojom::SharedHttpCacheClientFactory> factory_receiver =
      factory_remote.InitWithNewPipeAndPassReceiver();

  MojoSharedHttpCacheClientRemote client_remote(std::move(factory_remote));

  // Close the receiver endpoint and wait until `factory_remote_.is_connected()`
  // becomes false before `SetDisconnectHandler()` is called.
  factory_receiver.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !client_remote.is_factory_connected_for_testing(); }));

  base::test::TestFuture<void> disconnect_future;
  client_remote.SetDisconnectHandler(disconnect_future.GetCallback());

  // The disconnect callback must not run synchronously inside
  // `SetDisconnectHandler()`.
  EXPECT_FALSE(disconnect_future.IsReady());

  // Running pending tasks should execute the posted `OnDisconnected()` call.
  EXPECT_TRUE(disconnect_future.Wait());
}

// Tests that once `Initialize()` is called, closing the factory receiver does
// not trigger the disconnect handler, and closing the client receiver does.
TEST_F(MojoSharedHttpCacheClientRemoteTest,
       LifecycleTransfersToClientRemoteAfterInitialize) {
  TestSharedHttpCacheClientFactory test_factory;
  mojo::Receiver<mojom::SharedHttpCacheClientFactory> factory_receiver(
      &test_factory);

  MojoSharedHttpCacheClientRemote client_remote(
      factory_receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<void> disconnect_future;
  client_remote.SetDisconnectHandler(disconnect_future.GetCallback());

  client_remote.Initialize(CreateDummyPendingFileSet());
  mojo::PendingReceiver<mojom::SharedHttpCacheClient> client_receiver =
      test_factory.TakeClientReceiver();
  EXPECT_TRUE(client_receiver.is_valid());

  // Destroying the single-use factory receiver should NOT trigger disconnect.
  factory_receiver.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !client_remote.is_factory_connected_for_testing(); }));
  EXPECT_FALSE(disconnect_future.IsReady());

  // Destroying the client receiver SHOULD trigger disconnect.
  client_receiver.reset();
  EXPECT_TRUE(disconnect_future.Wait());
}

}  // namespace
}  // namespace network
