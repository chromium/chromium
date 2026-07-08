// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/device_bound_session_service_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class CertRequestInterceptingObserver : public TestURLLoaderNetworkObserver {
 public:
  using OnCertRequestedCallback = base::RepeatingCallback<void(
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder> responder)>;

  explicit CertRequestInterceptingObserver(OnCertRequestedCallback callback)
      : callback_(std::move(callback)) {}

  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder>
          client_cert_responder) override {
    callback_.Run(cert_info, std::move(client_cert_responder));
  }

 private:
  OnCertRequestedCallback callback_;
};

// A dummy SSLPrivateKey for testing.
class DummySSLPrivateKey : public mojom::SSLPrivateKey {
 public:
  DummySSLPrivateKey() = default;
  ~DummySSLPrivateKey() override = default;

  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            SignCallback callback) override {
    std::move(callback).Run(net::OK, {});
  }
};

class DeviceBoundSessionServiceDelegateTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

TEST_F(DeviceBoundSessionServiceDelegateTest,
       SelectClientCertificate_CertSelected) {
  scoped_refptr<net::X509Certificate> test_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(test_cert);

  base::RunLoop run_loop;
  auto observer =
      std::make_unique<CertRequestInterceptingObserver>(base::BindRepeating(
          [](scoped_refptr<net::X509Certificate> cert,
             const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
             mojo::PendingRemote<mojom::ClientCertificateResponder>
                 responder_remote) {
            mojo::Remote<mojom::ClientCertificateResponder> responder(
                std::move(responder_remote));
            mojo::PendingRemote<mojom::SSLPrivateKey> ssl_private_key;
            mojo::MakeSelfOwnedReceiver(
                std::make_unique<DummySSLPrivateKey>(),
                ssl_private_key.InitWithNewPipeAndPassReceiver());
            responder->ContinueWithCertificate(cert, "DummyProvider", {1, 2},
                                               std::move(ssl_private_key));
          },
          test_cert));

  DeviceBoundSessionServiceDelegate delegate(observer->Bind());

  scoped_refptr<net::SSLCertRequestInfo> cert_info =
      base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::test::TestFuture<scoped_refptr<net::X509Certificate>,
                         scoped_refptr<net::SSLPrivateKey>, bool>
      future;

  delegate.SelectClientCertificate(GURL("https://example.com"), cert_info,
                                   future.GetCallback());

  auto [cert, key, cancel] = future.Take();
  EXPECT_FALSE(cancel);
  ASSERT_NE(cert, nullptr);
  EXPECT_TRUE(test_cert->EqualsIncludingChain(cert.get()));
  EXPECT_NE(key, nullptr);
}

TEST_F(DeviceBoundSessionServiceDelegateTest,
       SelectClientCertificate_NoCertSelected) {
  base::RunLoop run_loop;
  auto observer =
      std::make_unique<CertRequestInterceptingObserver>(base::BindRepeating(
          [](const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
             mojo::PendingRemote<mojom::ClientCertificateResponder>
                 responder_remote) {
            mojo::Remote<mojom::ClientCertificateResponder> responder(
                std::move(responder_remote));
            responder->ContinueWithoutCertificate();
          }));

  DeviceBoundSessionServiceDelegate delegate(observer->Bind());

  scoped_refptr<net::SSLCertRequestInfo> cert_info =
      base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::test::TestFuture<scoped_refptr<net::X509Certificate>,
                         scoped_refptr<net::SSLPrivateKey>, bool>
      future;

  delegate.SelectClientCertificate(GURL("https://example.com"), cert_info,
                                   future.GetCallback());

  auto [cert, key, cancel] = future.Take();
  EXPECT_FALSE(cancel);
  EXPECT_EQ(cert, nullptr);
  EXPECT_EQ(key, nullptr);
}

TEST_F(DeviceBoundSessionServiceDelegateTest,
       SelectClientCertificate_CancelSelected) {
  base::RunLoop run_loop;
  auto observer =
      std::make_unique<CertRequestInterceptingObserver>(base::BindRepeating(
          [](const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
             mojo::PendingRemote<mojom::ClientCertificateResponder>
                 responder_remote) {
            mojo::Remote<mojom::ClientCertificateResponder> responder(
                std::move(responder_remote));
            responder->CancelRequest();
          }));

  DeviceBoundSessionServiceDelegate delegate(observer->Bind());

  scoped_refptr<net::SSLCertRequestInfo> cert_info =
      base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::test::TestFuture<scoped_refptr<net::X509Certificate>,
                         scoped_refptr<net::SSLPrivateKey>, bool>
      future;

  delegate.SelectClientCertificate(GURL("https://example.com"), cert_info,
                                   future.GetCallback());

  auto [cert, key, cancel] = future.Take();
  EXPECT_TRUE(cancel);
  EXPECT_EQ(cert, nullptr);
  EXPECT_EQ(key, nullptr);
}

TEST_F(DeviceBoundSessionServiceDelegateTest,
       SelectClientCertificate_MojoDisconnected) {
  base::RunLoop run_loop;
  auto observer =
      std::make_unique<CertRequestInterceptingObserver>(base::BindRepeating(
          [](const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
             mojo::PendingRemote<mojom::ClientCertificateResponder>
                 responder_remote) {
            // Disconnect immediately.
          }));

  DeviceBoundSessionServiceDelegate delegate(observer->Bind());

  scoped_refptr<net::SSLCertRequestInfo> cert_info =
      base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::test::TestFuture<scoped_refptr<net::X509Certificate>,
                         scoped_refptr<net::SSLPrivateKey>, bool>
      future;

  delegate.SelectClientCertificate(GURL("https://example.com"), cert_info,
                                   future.GetCallback());

  auto [cert, key, cancel] = future.Take();
  EXPECT_TRUE(cancel);
  EXPECT_EQ(cert, nullptr);
  EXPECT_EQ(key, nullptr);
}

TEST_F(DeviceBoundSessionServiceDelegateTest,
       SelectClientCertificate_NoObserver) {
  base::RunLoop run_loop;
  DeviceBoundSessionServiceDelegate delegate{mojo::NullRemote()};

  scoped_refptr<net::SSLCertRequestInfo> cert_info =
      base::MakeRefCounted<net::SSLCertRequestInfo>();

  base::test::TestFuture<scoped_refptr<net::X509Certificate>,
                         scoped_refptr<net::SSLPrivateKey>, bool>
      future;

  delegate.SelectClientCertificate(GURL("https://example.com"), cert_info,
                                   future.GetCallback());

  auto [cert, key, cancel] = future.Take();
  EXPECT_TRUE(cancel);
  EXPECT_EQ(cert, nullptr);
  EXPECT_EQ(key, nullptr);
}

}  // namespace

}  // namespace network
