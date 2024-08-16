// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_factory_builder.h"

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using ::testing::ElementsAre;

// This file explains the usage and behavior of `URLLoaderFactoryBuilder` and
// related classes, in the form of working unit tests.

class URLLoaderFactoryBuilderTest : public testing::Test {
 public:
  URLLoaderFactoryBuilderTest() = default;
  ~URLLoaderFactoryBuilderTest() override = default;

  void AddLog(std::string log) { logs_.push_back(std::move(log)); }
  const std::vector<std::string>& GetLogs() const { return logs_; }

  void AddInterceptors(URLLoaderFactoryBuilder& factory_builder,
                       int num_interceptors);

  void KeepAlive(scoped_refptr<SharedURLLoaderFactory> interceptor) {
    interceptors_.push_back(std::move(interceptor));
  }

  base::WeakPtr<URLLoaderFactoryBuilderTest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  template <typename T>
  void CreateLoaderAndStart(const T& result) {
    mojo::PendingRemote<mojom::URLLoader> loader;
    mojo::PendingReceiver<mojom::URLLoaderClient> client;

    result->CreateLoaderAndStart(loader.InitWithNewPipeAndPassReceiver(),
                                 0 /* request_id */, 0 /* options */,
                                 ResourceRequest(),
                                 client.InitWithNewPipeAndPassRemote(),
                                 net::MutableNetworkTrafficAnnotationTag());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::vector<std::string> logs_;

  // A std::vector just to keep alive the factories until the end of the test.
  std::vector<scoped_refptr<SharedURLLoaderFactory>> interceptors_;

  base::WeakPtrFactory<URLLoaderFactoryBuilderTest> weak_factory_{this};
};

class LoggingURLLoaderFactory final : public SharedURLLoaderFactory {
 public:
  LoggingURLLoaderFactory(URLLoaderFactoryBuilderTest* test,
                          std::string name,
                          mojo::PendingRemote<mojom::URLLoaderFactory> target,
                          base::OnceClosure on_create_loader_and_start)
      : test_(test->GetWeakPtr()),
        name_(std::move(name)),
        target_(std::move(target)),
        on_create_loader_and_start_(std::move(on_create_loader_and_start)) {}

  // Create a URLLoaderFactory that routes incoming requests to `target`.
  static scoped_refptr<SharedURLLoaderFactory> Create(
      URLLoaderFactoryBuilderTest* test,
      std::string name,
      mojo::PendingRemote<mojom::URLLoaderFactory> target,
      base::OnceClosure on_create_loader_and_start = {}) {
    auto factory = base::MakeRefCounted<LoggingURLLoaderFactory>(
        test, std::move(name), std::move(target),
        std::move(on_create_loader_and_start));
    test->KeepAlive(factory);
    return factory;
  }

  // Create a terminal URLLoaderFactory.
  static scoped_refptr<SharedURLLoaderFactory> CreateTerminal(
      URLLoaderFactoryBuilderTest* test,
      std::string name,
      base::OnceClosure on_create_loader_and_start) {
    return Create(test, std::move(name), mojo::NullRemote(),
                  std::move(on_create_loader_and_start));
  }

  static mojo::PendingRemote<mojom::URLLoaderFactory>
  CreateTerminalPendingRemote(URLLoaderFactoryBuilderTest* test,
                              std::string name,
                              base::OnceClosure on_create_loader_and_start) {
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver =
        remote.InitWithNewPipeAndPassReceiver();

    CreateTerminal(test, std::move(name), std::move(on_create_loader_and_start))
        ->Clone(std::move(receiver));
    return remote;
  }

  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> loader,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    CHECK(test_);
    test_->AddLog(name_);
    if (target_) {
      target_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                    request, std::move(client),
                                    traffic_annotation);
    }
    if (on_create_loader_and_start_) {
      std::move(on_create_loader_and_start_).Run();
    }
  }

  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  std::unique_ptr<PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
  }

 private:
  ~LoggingURLLoaderFactory() override = default;

  base::WeakPtr<URLLoaderFactoryBuilderTest> test_;
  std::string name_;

  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
  mojo::Remote<mojom::URLLoaderFactory> target_;
  base::OnceClosure on_create_loader_and_start_;
};

class LoggingNetworkContext final : public TestNetworkContext {
 public:
  LoggingNetworkContext(URLLoaderFactoryBuilderTest* test,
                        std::string name,
                        base::OnceClosure on_create_loader_and_start)
      : test_(test->GetWeakPtr()),
        name_(std::move(name)),
        on_create_loader_and_start_(std::move(on_create_loader_and_start)) {}

  void CreateURLLoaderFactory(
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      mojom::URLLoaderFactoryParamsPtr factory_param) override {
    LoggingURLLoaderFactory::CreateTerminal(
        test_.get(), name_, std::move(on_create_loader_and_start_))
        ->Clone(std::move(receiver));
  }

 private:
  base::WeakPtr<URLLoaderFactoryBuilderTest> test_;
  std::string name_;
  base::OnceClosure on_create_loader_and_start_;
};

// ================================================================
// Basic URLLoaderFactoryBuilder usage with `Finish()`.

TEST_F(URLLoaderFactoryBuilderTest, Basic) {
  // Start with no interceptors.
  URLLoaderFactoryBuilder factory_builder;

  // Append the first intercepting step.
  {
    // Connect the pending receiver/remote returned by
    // `URLLoaderFactoryBuilder::Append()`.
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver;
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    std::tie(receiver, remote) = factory_builder.Append();

    scoped_refptr<SharedURLLoaderFactory> interceptor =
        LoggingURLLoaderFactory::Create(this, "Interceptor1",
                                        std::move(remote));
    interceptor->Clone(std::move(receiver));
  }

  // Append the second intercepting step.
  {
    // Connect the pending receiver/remote returned by
    // `URLLoaderFactoryBuilder::Append()`.
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver;
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    std::tie(receiver, remote) = factory_builder.Append();

    scoped_refptr<SharedURLLoaderFactory> interceptor =
        LoggingURLLoaderFactory::Create(this, "Interceptor2",
                                        std::move(remote));
    interceptor->Clone(std::move(receiver));
  }

  // After all intercepting steps are added, finish the builder with the
  // terminal `URLLoaderFactory` to get the resulting `URLLoaderFactory`.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder).Finish(std::move(terminal));

  // Requests to the resulting `URLLoaderFactory` `result` goes through the
  // interceptors in the order of `Append()` calls, then finally to `terminal`.
  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

TEST_F(URLLoaderFactoryBuilderTest, Empty) {
  // Start with no interceptors.
  URLLoaderFactoryBuilder factory_builder;

  // We can finish the builder using the same interface even when it has no
  // interceptors.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder).Finish(std::move(terminal));

  // Requests to the resulting `URLLoaderFactory` `result` goes to `terminal`.
  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(), ElementsAre("Terminal"));
}

// ================================================================
// Basic URLLoaderFactoryBuilder usage with `Finish()` with `PendingReceiver`.

TEST_F(URLLoaderFactoryBuilderTest, BasicWithPendingReceiver) {
  // We are given an `mojo::PendingReceiver` to start with.
  mojo::Remote<mojom::URLLoaderFactory> root_remote;
  mojo::PendingReceiver<mojom::URLLoaderFactory> root_receiver =
      root_remote.BindNewPipeAndPassReceiver();

  // Start with an empty builder.
  URLLoaderFactoryBuilder factory_builder;

  // Append the first intercepting step.
  {
    // Connect the pending receiver/remote returned by
    // `URLLoaderFactoryBuilder::Append()`.
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver;
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    std::tie(receiver, remote) = factory_builder.Append();

    scoped_refptr<SharedURLLoaderFactory> interceptor =
        LoggingURLLoaderFactory::Create(this, "Interceptor1",
                                        std::move(remote));
    interceptor->Clone(std::move(receiver));
  }

  // Append the second intercepting step.
  {
    // Connect the pending receiver/remote returned by
    // `URLLoaderFactoryBuilder::Append()`.
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver;
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    std::tie(receiver, remote) = factory_builder.Append();

    scoped_refptr<SharedURLLoaderFactory> interceptor =
        LoggingURLLoaderFactory::Create(this, "Interceptor2",
                                        std::move(remote));
    interceptor->Clone(std::move(receiver));
  }

  // After all intercepting steps are added, finish the builder with the
  // terminal `URLLoaderFactory` and connect it to `root_receiver`, instead of
  // returning `scoped_refptr<SharedURLLoaderFactory>` etc.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  std::move(factory_builder)
      .Finish(std::move(root_receiver), std::move(terminal));

  // Requests to the initially given `mojo::PendingReceiver` (i.e. via
  // `root_remote` / `root_receiver`) goes through the interceptors in the order
  // of `Append()` calls, then finally to `terminal`.
  CreateLoaderAndStart(root_remote);
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

TEST_F(URLLoaderFactoryBuilderTest, EmptyWithPendingReceiver) {
  // We are given an `mojo::PendingReceiver` to start with.
  mojo::Remote<mojom::URLLoaderFactory> root_remote;
  mojo::PendingReceiver<mojom::URLLoaderFactory> root_receiver =
      root_remote.BindNewPipeAndPassReceiver();

  // Start with an empty builder.
  URLLoaderFactoryBuilder factory_builder;

  // We can finish the builder using the same interface even when the builder
  // is empty.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  std::move(factory_builder)
      .Finish(std::move(root_receiver), std::move(terminal));

  // Requests to the initially given `mojo::PendingReceiver` (i.e. via
  // `root_remote` / `root_receiver`) goes to `terminal`.
  CreateLoaderAndStart(root_remote);
  run_loop.Run();
  EXPECT_THAT(GetLogs(), ElementsAre("Terminal"));
}

// ================================================================
// Passing `URLLoaderFactoryBuilder&` to allow adding interceptors only.

// A common scenario is that interceptors are added somewhere behind an
// interface like `ContentBrowserClient::WillCreateURLLoaderFactory` while the
// caller have the full control over creating the resulting `URLLoaderFactory`
// except for adding interceptors in the middle. In such scenarios, we can
// pass `URLLoaderFactoryBuilder&` that only allows appending interceptors
// (and doesn't allow finishing the builder nor destructing the builder
// object) to the intercepting interface.
void URLLoaderFactoryBuilderTest::AddInterceptors(
    URLLoaderFactoryBuilder& factory_builder,
    int num_interceptors) {
  // Add interceptors, named "Interceptor1", "Interceptor2", ...
  for (int i = 0; i < num_interceptors; ++i) {
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver;
    mojo::PendingRemote<mojom::URLLoaderFactory> remote;
    std::tie(receiver, remote) = factory_builder.Append();

    scoped_refptr<SharedURLLoaderFactory> interceptor =
        LoggingURLLoaderFactory::Create(
            this, "Interceptor" + base::NumberToString(i + 1),
            std::move(remote));
    interceptor->Clone(std::move(receiver));
  }
}

TEST_F(URLLoaderFactoryBuilderTest, AddInterceptorsByReference) {
  // Start with no interceptors.
  URLLoaderFactoryBuilder factory_builder;

  // Two interceptors are added via `URLLoaderFactoryBuilder&`.
  // Note that the caller here don't have to know whether or how many
  // interceptors are actually added.
  AddInterceptors(factory_builder, 2);

  // Finish the builder with the terminal `URLLoaderFactory` to get the
  // resulting `URLLoaderFactory`.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder).Finish(std::move(terminal));

  // Requests to the resulting `URLLoaderFactory` `result` goes through the
  // interceptors in the order of `Append()` calls, then finally to `terminal`.
  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

// The caller code remains the same even when no interceptors are added.
TEST_F(URLLoaderFactoryBuilderTest, AddNoInterceptorsByReference) {
  // Start with no interceptors.
  URLLoaderFactoryBuilder factory_builder;

  // No interceptors are added here, but the caller-side code can remain the
  // same.
  AddInterceptors(factory_builder, 0);

  // Finish the builder with the terminal `URLLoaderFactory` to get the
  // resulting `URLLoaderFactory`.
  base::RunLoop run_loop;
  scoped_refptr<SharedURLLoaderFactory> terminal =
      LoggingURLLoaderFactory::CreateTerminal(this, "Terminal",
                                              run_loop.QuitClosure());
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder).Finish(std::move(terminal));

  // Requests to the resulting `URLLoaderFactory` `result` goes through the
  // interceptors in the order of `Append()` calls, then finally to `terminal`.
  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(), ElementsAre("Terminal"));
}

// ================================================================
// More URLLoaderFactory-ish types support.
// In the tests above, all interceptors, terminals and resulting endpoints
// (returned by `Finish()`) are `SharedURLLoaderFactory`, but we can use other
// similare interfaces.
TEST_F(URLLoaderFactoryBuilderTest, URLLoaderFactoryFromTerminalPendingRemote) {
  URLLoaderFactoryBuilder factory_builder;
  AddInterceptors(factory_builder, 2);

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::URLLoaderFactory> terminal(
      LoggingURLLoaderFactory::CreateTerminalPendingRemote(
          this, "Terminal", run_loop.QuitClosure()));
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder).Finish(std::move(terminal));

  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

TEST_F(URLLoaderFactoryBuilderTest,
       URLLoaderFactoryFromTerminalNetworkContext) {
  URLLoaderFactoryBuilder factory_builder;
  AddInterceptors(factory_builder, 2);

  base::RunLoop run_loop;
  auto terminal = std::make_unique<LoggingNetworkContext>(
      this, "Terminal", run_loop.QuitClosure());
  scoped_refptr<SharedURLLoaderFactory> result =
      std::move(factory_builder)
          .Finish(terminal.get(), mojom::URLLoaderFactoryParams::New());

  CreateLoaderAndStart(result);
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

// Return PendingRemote instead of URLLoaderFactory.
// See or add the `URLLoaderFactoryBuilder::WrapAs()` for actual implementation.
TEST_F(URLLoaderFactoryBuilderTest, PendingRemoteFromTerminalPendingRemote) {
  URLLoaderFactoryBuilder factory_builder;
  AddInterceptors(factory_builder, 2);

  base::RunLoop run_loop;
  mojo::PendingRemote<mojom::URLLoaderFactory> terminal(
      LoggingURLLoaderFactory::CreateTerminalPendingRemote(
          this, "Terminal", run_loop.QuitClosure()));
  mojo::PendingRemote<mojom::URLLoaderFactory> result =
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<mojom::URLLoaderFactory>>(
              std::move(terminal));

  CreateLoaderAndStart(
      mojo::Remote<mojom::URLLoaderFactory>(std::move(result)));
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

TEST_F(URLLoaderFactoryBuilderTest, PendingRemoteFromTerminalNetworkContext) {
  URLLoaderFactoryBuilder factory_builder;
  AddInterceptors(factory_builder, 2);

  base::RunLoop run_loop;
  auto terminal = std::make_unique<LoggingNetworkContext>(
      this, "Terminal", run_loop.QuitClosure());
  mojo::PendingRemote<mojom::URLLoaderFactory> result =
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<mojom::URLLoaderFactory>>(
              terminal.get(), mojom::URLLoaderFactoryParams::New());

  CreateLoaderAndStart(
      mojo::Remote<mojom::URLLoaderFactory>(std::move(result)));
  run_loop.Run();
  EXPECT_THAT(GetLogs(),
              ElementsAre("Interceptor1", "Interceptor2", "Terminal"));
}

}  // namespace
}  // namespace network
