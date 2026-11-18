// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/browser_interop.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "remoting/base/url_loader_network_service_observer.h"
#include "remoting/host/chromoting_host_context.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Receives the response to a certificate request, like the network service
// does.
class FakeClientCertificateResponder
    : public network::mojom::ClientCertificateResponder {
 public:
  explicit FakeClientCertificateResponder(
      base::OnceCallback<void(bool)> on_response)
      : on_response_(std::move(on_response)) {}

  mojo::PendingRemote<network::mojom::ClientCertificateResponder>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    // Unretained is safe since `receiver_` is owned by `this`.
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakeClientCertificateResponder::OnDisconnected,
                       base::Unretained(this)));
    return remote;
  }

  // network::mojom::ClientCertificateResponder implementation:
  void ContinueWithCertificate(
      const scoped_refptr<net::X509Certificate>& x509_certificate,
      const std::string& provider_name,
      const std::vector<uint16_t>& algorithm_preferences,
      mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key)
      override {
    Notify(true);
  }
  void ContinueWithoutCertificate() override { Notify(true); }
  void CancelRequest() override { Notify(false); }

 private:
  void OnDisconnected() {
    // The observer closes the pipe once it has responded.
    Notify(false);
  }

  void Notify(bool continued) {
    if (on_response_) {
      std::move(on_response_).Run(continued);
    }
  }

  mojo::Receiver<network::mojom::ClientCertificateResponder> receiver_{this};
  base::OnceCallback<void(bool)> on_response_;
};

// Owns the Mojo remotes and responders on the Browser IO thread, matching
// ProtobufHttpClient's network thread usage on ChromeOS.
struct IoThreadCertState {
  base::SequenceBound<UrlLoaderNetworkServiceObserver> observer;
  mojo::Remote<network::mojom::URLLoaderNetworkServiceObserver> observer_remote;
  std::vector<std::unique_ptr<FakeClientCertificateResponder>> responders;
};

}  // namespace

using BrowserInteropBrowserTest = InProcessBrowserTest;

// The client cert store on ChromeOS is backed by objects that are bound to the
// UI thread (Profile, keyed services, Kcer), while CRD makes its requests on
// its network thread, which is the browser IO thread. Thread checks fail if
// the store is used on the wrong thread.
IN_PROC_BROWSER_TEST_F(BrowserInteropBrowserTest,
                       ClientCertificateRequestsAreHandled) {
  ASSERT_TRUE(GetProfile());
  auto browser_interop = base::MakeRefCounted<BrowserInterop>();
  std::unique_ptr<ChromotingHostContext> host_context =
      browser_interop->CreateChromotingHostContext(GetProfile());
  scoped_refptr<base::SequencedTaskRunner> cert_store_task_runner =
      host_context->client_cert_store_task_runner();
  // The test runs on the UI thread.
  ASSERT_TRUE(cert_store_task_runner);
  EXPECT_TRUE(cert_store_task_runner->RunsTasksInCurrentSequence());

  auto io_state = std::make_unique<IoThreadCertState>();
  IoThreadCertState* io_state_ptr = io_state.get();
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      content::GetIOThreadTaskRunner({});

  base::test::TestFuture<void> io_ready;
  io_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](IoThreadCertState* state,
             scoped_refptr<base::SequencedTaskRunner> cert_store_runner,
             ChromotingHostContext::CreateClientCertStoreCallback
                 create_cert_store) {
            state->observer =
                base::SequenceBound<UrlLoaderNetworkServiceObserver>(
                    std::move(cert_store_runner), std::move(create_cert_store));
            state->observer
                .AsyncCall(&UrlLoaderNetworkServiceObserver::BindReceiver)
                .WithArgs(state->observer_remote.BindNewPipeAndPassReceiver());
          },
          io_state_ptr, cert_store_task_runner,
          host_context->create_client_cert_store_callback()),
      io_ready.GetCallback());
  ASSERT_TRUE(io_ready.Wait());

  // The test profile has no client certs. What matters is that each sequential
  // request from the IO thread gets a response without failing a thread check.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);
    base::test::TestFuture<bool> continued;
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](IoThreadCertState* state,
               base::OnceCallback<void(bool)> on_response) {
              auto responder = std::make_unique<FakeClientCertificateResponder>(
                  std::move(on_response));
              state->observer_remote->OnCertificateRequested(
                  std::nullopt, base::MakeRefCounted<net::SSLCertRequestInfo>(),
                  responder->BindNewPipeAndPassRemote());
              state->responders.push_back(std::move(responder));
            },
            io_state_ptr,
            base::BindPostTaskToCurrentDefault(continued.GetCallback())));
    EXPECT_TRUE(continued.Get());
  }

  // Tear down the Mojo endpoints and SequenceBound observer on the IO thread,
  // then flush the UI thread so observer and store destruction finishes
  // cleanly.
  base::test::TestFuture<void> io_cleaned_up;
  io_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<IoThreadCertState> state) {},
                     std::move(io_state)),
      io_cleaned_up.GetCallback());
  ASSERT_TRUE(io_cleaned_up.Wait());

  base::test::TestFuture<void> observer_destroyed;
  cert_store_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           observer_destroyed.GetCallback());
  EXPECT_TRUE(observer_destroyed.Wait());
}

}  // namespace remoting
