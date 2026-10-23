// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/cross_thread_network_context.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/unguessable_token.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/reconnect_notifier.h"
#include "net/base/schemeful_site.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {
namespace {

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;
  ~FakeNetworkContext() override = default;

  void Clone(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void AddQuicHints(
      const std::vector<url::SchemeHostPort>& origins,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    for (const auto& origin : origins) {
      quic_hint_origins_.push_back(origin);
    }
    if (on_add_quic_hints_) {
      on_add_quic_hints_.Run();
    }
  }

  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const base::UnguessableToken& network_restrictions_id,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const std::optional<net::ConnectionKeepAliveConfig>& keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client) override {
    num_streams_ = num_streams;
    preconnected_urls_.push_back(url);
    credentials_mode_ = credentials_mode;
    network_anonymization_key_ = network_anonymization_key;
    network_restrictions_id_ = network_restrictions_id;
    if (on_preconnect_sockets_) {
      on_preconnect_sockets_.Run();
    }
  }

  void DisconnectAllReceivers() { receivers_.Clear(); }

  size_t receiver_count() const { return receivers_.size(); }
  uint32_t num_streams() const { return num_streams_; }
  network::mojom::CredentialsMode credentials_mode() const {
    return credentials_mode_;
  }
  const net::NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }
  const base::UnguessableToken& network_restrictions_id() const {
    return network_restrictions_id_;
  }
  const std::vector<url::SchemeHostPort>& quic_hint_origins() const {
    return quic_hint_origins_;
  }
  const std::vector<GURL>& preconnected_urls() const {
    return preconnected_urls_;
  }

  void set_on_add_quic_hints(base::RepeatingClosure callback) {
    on_add_quic_hints_ = std::move(callback);
  }
  void set_on_preconnect_sockets(base::RepeatingClosure callback) {
    on_preconnect_sockets_ = std::move(callback);
  }

 private:
  mojo::ReceiverSet<network::mojom::NetworkContext> receivers_;
  uint32_t num_streams_ = 0;
  network::mojom::CredentialsMode credentials_mode_ =
      network::mojom::CredentialsMode::kOmit;
  net::NetworkAnonymizationKey network_anonymization_key_;
  base::UnguessableToken network_restrictions_id_;
  std::vector<url::SchemeHostPort> quic_hint_origins_;
  std::vector<GURL> preconnected_urls_;
  base::RepeatingClosure on_add_quic_hints_;
  base::RepeatingClosure on_preconnect_sockets_;
};

class CrossThreadNetworkContextTest : public testing::Test {
 public:
  scoped_refptr<CrossThreadNetworkContext> CreateContext() {
    return CrossThreadNetworkContext::CreateForTesting(
        base::BindRepeating(&FakeNetworkContext::Clone,
                            base::Unretained(&fake_network_context_)),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  FakeNetworkContext fake_network_context_;
};

TEST_F(CrossThreadNetworkContextTest, CallFromBackgroundSequence) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop run_loop;
  fake_network_context_.set_on_add_quic_hints(run_loop.QuitClosure());

  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([context]() {
        url::SchemeHostPort origin("https", "example.com", 443);
        context->AddQuicHints({origin}, net::NetworkAnonymizationKey());
      }));

  run_loop.Run();

  ASSERT_EQ(1u, fake_network_context_.quic_hint_origins().size());
  EXPECT_EQ("example.com", fake_network_context_.quic_hint_origins()[0].host());
  EXPECT_EQ(443, fake_network_context_.quic_hint_origins()[0].port());
}

TEST_F(CrossThreadNetworkContextTest, PreconnectSocketsFromBackgroundSequence) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop run_loop;
  fake_network_context_.set_on_preconnect_sockets(run_loop.QuitClosure());

  const GURL url("https://example.com");
  const auto nak =
      net::NetworkAnonymizationKey::CreateCrossSite(net::SchemefulSite(url));
  const auto token = base::UnguessableToken::Create();

  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([context, url, nak, token]() {
        context->PreconnectSockets(
            2, url, network::mojom::CredentialsMode::kInclude, nak, token,
            net::MutableNetworkTrafficAnnotationTag(
                TRAFFIC_ANNOTATION_FOR_TESTS),
            std::nullopt, mojo::NullRemote());
      }));

  run_loop.Run();

  ASSERT_EQ(1u, fake_network_context_.preconnected_urls().size());
  EXPECT_EQ("https://example.com/",
            fake_network_context_.preconnected_urls()[0].spec());
  EXPECT_EQ(2u, fake_network_context_.num_streams());
  EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
            fake_network_context_.credentials_mode());
  EXPECT_EQ(nak, fake_network_context_.network_anonymization_key());
  EXPECT_EQ(token, fake_network_context_.network_restrictions_id());
}

TEST_F(CrossThreadNetworkContextTest, LazyReconnectOnCrash) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  // 1. Initial call from background thread.
  {
    base::RunLoop run_loop;
    fake_network_context_.set_on_add_quic_hints(run_loop.QuitClosure());

    background_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([context]() {
          url::SchemeHostPort origin("https", "first.com", 443);
          context->AddQuicHints({origin}, net::NetworkAnonymizationKey());
        }));

    run_loop.Run();
    EXPECT_EQ(1u, fake_network_context_.quic_hint_origins().size());
    EXPECT_EQ(1u, fake_network_context_.receiver_count());
  }

  // 2. Simulate NetworkService crash by disconnecting all receivers.
  fake_network_context_.DisconnectAllReceivers();
  EXPECT_EQ(0u, fake_network_context_.receiver_count());

  // Deterministically flush the background remote to observe disconnection.
  {
    base::RunLoop run_loop;
    background_runner->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([context, quit = run_loop.QuitClosure()]() {
          context->FlushAsyncForTesting(quit);
        }));
    run_loop.Run();
  }

  // 3. Next calls on background thread during crash recovery buffer and
  // automatically rebind when the UI thread processes the clone callback.
  {
    base::RunLoop run_loop;
    fake_network_context_.set_on_add_quic_hints(
        base::BarrierClosure(2, run_loop.QuitClosure()));

    background_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([context]() {
          url::SchemeHostPort origin2("https", "second.com", 443);
          url::SchemeHostPort origin3("https", "third.com", 443);
          context->AddQuicHints({origin2}, net::NetworkAnonymizationKey());
          context->AddQuicHints({origin3}, net::NetworkAnonymizationKey());
        }));

    run_loop.Run();
    ASSERT_EQ(3u, fake_network_context_.quic_hint_origins().size());
    EXPECT_THAT(
        fake_network_context_.quic_hint_origins(),
        testing::ElementsAre(url::SchemeHostPort("https", "first.com", 443),
                             url::SchemeHostPort("https", "second.com", 443),
                             url::SchemeHostPort("https", "third.com", 443)));
    EXPECT_EQ(1u, fake_network_context_.receiver_count());
  }
}

TEST_F(CrossThreadNetworkContextTest, MultipleCallsShareSinglePipe) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop run_loop;
  fake_network_context_.set_on_add_quic_hints(
      base::BarrierClosure(3, run_loop.QuitClosure()));

  // Issue 3 calls back-to-back on the background sequence to verify they share
  // a single cloned pipe.
  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([context]() {
        for (int i = 1; i <= 3; ++i) {
          url::SchemeHostPort origin("https",
                                     base::StringPrintf("site%d.com", i), 443);
          context->AddQuicHints({origin}, net::NetworkAnonymizationKey());
        }
      }));

  run_loop.Run();

  EXPECT_THAT(
      fake_network_context_.quic_hint_origins(),
      testing::ElementsAre(url::SchemeHostPort("https", "site1.com", 443),
                           url::SchemeHostPort("https", "site2.com", 443),
                           url::SchemeHostPort("https", "site3.com", 443)));
  EXPECT_EQ(1u, fake_network_context_.receiver_count());
}

TEST_F(CrossThreadNetworkContextTest, UnusedContextDestructionClosesReceiver) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  context.reset();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return fake_network_context_.receiver_count() == 0u; }));
}

TEST_F(CrossThreadNetworkContextTest, CrossSequenceDestruction) {
  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop run_loop;
  fake_network_context_.set_on_add_quic_hints(run_loop.QuitClosure());

  // Pass an unowned pointer to avoid holding an extra reference in the
  // background task closure. `context` is kept alive on the UI thread.
  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([raw_context = context.get()]() {
        url::SchemeHostPort origin("https", "test.com", 443);
        raw_context->AddQuicHints({origin}, net::NetworkAnonymizationKey());
      }));

  run_loop.Run();
  EXPECT_EQ(1u, fake_network_context_.quic_hint_origins().size());

  // Ensure the background task has completely finished and unwound.
  base::RunLoop drain_loop;
  background_runner->PostTask(FROM_HERE, drain_loop.QuitClosure());
  drain_loop.Run();

  // Drop the sole reference on the UI thread. CrossThreadNetworkContext's
  // destructor should safely delete the Remote on the background runner via
  // DeleteSoon.
  context.reset();
  base::RunLoop delete_loop;
  background_runner->PostTask(FROM_HERE, delete_loop.QuitClosure());
  delete_loop.Run();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return fake_network_context_.receiver_count() == 0u; }));
}

TEST_F(CrossThreadNetworkContextTest, InSequenceDestruction) {
  scoped_refptr<base::SequencedTaskRunner> background_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  base::RunLoop run_loop;
  fake_network_context_.set_on_add_quic_hints(run_loop.QuitClosure());

  auto context = CreateContext();
  ASSERT_TRUE(context);
  EXPECT_EQ(1u, fake_network_context_.receiver_count());

  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([context = std::move(context)]() {
        url::SchemeHostPort origin("https", "test.com", 443);
        context->AddQuicHints({origin}, net::NetworkAnonymizationKey());
        // `context` is released on `background_runner` when lambda finishes.
      }));

  run_loop.Run();
  base::RunLoop background_drain_loop;
  background_runner->PostTask(FROM_HERE, background_drain_loop.QuitClosure());
  background_drain_loop.Run();
  EXPECT_EQ(1u, fake_network_context_.quic_hint_origins().size());

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return fake_network_context_.receiver_count() == 0u; }));
}

}  // namespace
}  // namespace content
