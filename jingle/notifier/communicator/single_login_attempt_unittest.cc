// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/single_login_attempt.h"

#include <cstddef>
#include <memory>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/glue/network_service_config_test_util.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/communicator/login_settings.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"

namespace jingle_xmpp {
class XmppTaskParentInterface;
}  // namespace jingle_xmpp

namespace notifier {

namespace {

enum DelegateState {
  IDLE, CONNECTED, REDIRECTED, CREDENTIALS_REJECTED, SETTINGS_EXHAUSTED
};

class FakeDelegate : public SingleLoginAttempt::Delegate {
 public:
  FakeDelegate() : state_(IDLE) {}

  void OnConnect(
      base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) override {
    state_ = CONNECTED;
    base_task_ = base_task;
  }

  void OnRedirect(const ServerInformation& redirect_server) override {
    state_ = REDIRECTED;
    redirect_server_ = redirect_server;
  }

  void OnCredentialsRejected() override { state_ = CREDENTIALS_REJECTED; }

  void OnSettingsExhausted() override { state_ = SETTINGS_EXHAUSTED; }

  DelegateState state() const { return state_; }

  base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task() const {
    return base_task_;
  }

  const ServerInformation& redirect_server() const {
    return redirect_server_;
  }

 private:
  DelegateState state_;
  base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task_;
  ServerInformation redirect_server_;
};

class MyTestURLRequestContext : public net::TestURLRequestContext {
 public:
  MyTestURLRequestContext() : TestURLRequestContext(true) {
    context_storage_.set_host_resolver(
        std::unique_ptr<net::HostResolver>(new net::HangingHostResolver()));
    Init();
  }
  ~MyTestURLRequestContext() override {}
};

class SingleLoginAttemptTest : public ::testing::Test {
 protected:
  SingleLoginAttemptTest()
      : net_config_helper_(
            base::MakeRefCounted<net::TestURLRequestContextGetter>(
                base::ThreadTaskRunnerHandle::Get(),
                std::unique_ptr<net::TestURLRequestContext>(
                    new MyTestURLRequestContext()))),
        login_settings_(
            jingle_xmpp::XmppClientSettings(),
            net_config_helper_.MakeSocketFactoryCallback(),
            ServerList(1,
                       ServerInformation(net::HostPortPair("example.com", 100),
                                         SUPPORTS_SSLTCP)),
            false /* try_ssltcp_first */,
            "auth_mechanism",
            TRAFFIC_ANNOTATION_FOR_TESTS),
        attempt_(new SingleLoginAttempt(login_settings_, &fake_delegate_)) {}

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  void FireRedirect(jingle_xmpp::XmlElement* redirect_error) {
    attempt_->OnError(jingle_xmpp::XmppEngine::ERROR_STREAM, 0, redirect_error);
  }

  ~SingleLoginAttemptTest() override {
    attempt_.reset();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  jingle_glue::NetworkServiceConfigTestUtil net_config_helper_;
  const LoginSettings login_settings_;

 protected:
  std::unique_ptr<SingleLoginAttempt> attempt_;
  FakeDelegate fake_delegate_;
  FakeBaseTask fake_base_task_;
};

// Fire OnConnect and make sure the base task gets passed to the
// delegate properly.
TEST_F(SingleLoginAttemptTest, Basic) {
  attempt_->OnConnect(fake_base_task_.AsWeakPtr());
  EXPECT_EQ(CONNECTED, fake_delegate_.state());
  EXPECT_EQ(fake_base_task_.AsWeakPtr().get(),
            fake_delegate_.base_task().get());
}

// Fire OnErrors and make sure the delegate gets the
// OnSettingsExhausted() event.
TEST_F(SingleLoginAttemptTest, Error) {
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(IDLE, fake_delegate_.state());
    attempt_->OnError(jingle_xmpp::XmppEngine::ERROR_NONE, 0, NULL);
  }
  EXPECT_EQ(SETTINGS_EXHAUSTED, fake_delegate_.state());
}

// Fire OnErrors but replace the last one with OnConnect, and make
// sure the delegate still gets the OnConnect message.
TEST_F(SingleLoginAttemptTest, ErrorThenSuccess) {
  attempt_->OnError(jingle_xmpp::XmppEngine::ERROR_NONE, 0, NULL);
  attempt_->OnConnect(fake_base_task_.AsWeakPtr());
  EXPECT_EQ(CONNECTED, fake_delegate_.state());
  EXPECT_EQ(fake_base_task_.AsWeakPtr().get(),
            fake_delegate_.base_task().get());
}

jingle_xmpp::XmlElement* MakeRedirectError(const std::string& redirect_server) {
  jingle_xmpp::XmlElement* stream_error =
      new jingle_xmpp::XmlElement(jingle_xmpp::QN_STREAM_ERROR, true);
  stream_error->AddElement(
      new jingle_xmpp::XmlElement(jingle_xmpp::QN_XSTREAM_SEE_OTHER_HOST, true));
  jingle_xmpp::XmlElement* text =
      new jingle_xmpp::XmlElement(jingle_xmpp::QN_XSTREAM_TEXT, true);
  stream_error->AddElement(text);
  text->SetBodyText(redirect_server);
  return stream_error;
}

// Fire a redirect and make sure the delegate gets the proper redirect
// server info.
TEST_F(SingleLoginAttemptTest, Redirect) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", 1000),
      SUPPORTS_SSLTCP);

  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.ToString()));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with the host only and make sure the delegate gets
// the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectHostOnly) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host()));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with a zero port and make sure the delegate gets
// the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectZeroPort) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host() + ":0"));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire a redirect with an invalid port and make sure the delegate
// gets the proper redirect server info with the default XMPP port.
TEST_F(SingleLoginAttemptTest, RedirectInvalidPort) {
  const ServerInformation redirect_server(
      net::HostPortPair("example.com", kDefaultXmppPort),
      SUPPORTS_SSLTCP);

  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(redirect_server.server.host() + ":invalidport"));
  FireRedirect(redirect_error.get());

  EXPECT_EQ(REDIRECTED, fake_delegate_.state());
  EXPECT_TRUE(fake_delegate_.redirect_server().Equals(redirect_server));
}

// Fire an empty redirect and make sure the delegate does not get a
// redirect.
TEST_F(SingleLoginAttemptTest, RedirectEmpty) {
  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(std::string()));
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

// Fire a redirect with a missing text element and make sure the
// delegate does not get a redirect.
TEST_F(SingleLoginAttemptTest, RedirectMissingText) {
  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(std::string()));
  redirect_error->RemoveChildAfter(redirect_error->FirstChild());
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

// Fire a redirect with a missing see-other-host element and make sure
// the delegate does not get a redirect.
TEST_F(SingleLoginAttemptTest, RedirectMissingSeeOtherHost) {
  std::unique_ptr<jingle_xmpp::XmlElement> redirect_error(
      MakeRedirectError(std::string()));
  redirect_error->RemoveChildAfter(NULL);
  FireRedirect(redirect_error.get());
  EXPECT_EQ(IDLE, fake_delegate_.state());
}

// Fire 'Unauthorized' errors and make sure the delegate gets the
// OnCredentialsRejected() event.
TEST_F(SingleLoginAttemptTest, CredentialsRejected) {
  attempt_->OnError(jingle_xmpp::XmppEngine::ERROR_UNAUTHORIZED, 0, NULL);
  EXPECT_EQ(CREDENTIALS_REJECTED, fake_delegate_.state());
}

}  // namespace

}  // namespace notifier
