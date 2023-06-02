// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

// The IPC channel doesn't work if we use a mock clock, so we just reduce the
// timeout to shorten the test time.
constexpr base::TimeDelta kTestRequestTimeout = base::Milliseconds(500);

class MockRemoteOpenUrlClientDelegate : public RemoteOpenUrlClient::Delegate {
 public:
  MOCK_METHOD(void, OpenUrlOnFallbackBrowser, (const GURL& url), (override));
  MOCK_METHOD(void, ShowOpenUrlError, (const GURL& url), (override));
};

class MockRemoteUrlOpener : public mojom::RemoteUrlOpener {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlCallback callback),
              (override));
};

base::CommandLine::StringType ToCommandLineString(const char* str) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(str);
#else
  return str;
#endif
}

}  // namespace

class RemoteOpenUrlClientTest : public testing::Test {
 public:
  RemoteOpenUrlClientTest();
  ~RemoteOpenUrlClientTest() override;

 protected:
  void BindMockRemoteUrlOpener();

  // These raw pointer objects are owned by |client_| so |client_| must outlive
  // the last use of them.
  raw_ptr<MockRemoteOpenUrlClientDelegate, DanglingUntriaged> delegate_;
  raw_ptr<MockChromotingHostServicesProvider, DanglingUntriaged> api_provider_;

  std::unique_ptr<RemoteOpenUrlClient> client_;
  MockChromotingSessionServices mock_api_;
  MockRemoteUrlOpener remote_url_opener_;
  mojo::Receiver<mojom::RemoteUrlOpener> remote_url_opener_receiver_{
      &remote_url_opener_};

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
};

RemoteOpenUrlClientTest::RemoteOpenUrlClientTest() {
  auto delegate = std::make_unique<MockRemoteOpenUrlClientDelegate>();
  delegate_ = delegate.get();
  auto api_provider = std::make_unique<MockChromotingHostServicesProvider>();
  api_provider_ = api_provider.get();
  client_ = base::WrapUnique(new RemoteOpenUrlClient(
      std::move(delegate), std::move(api_provider), kTestRequestTimeout));
}

RemoteOpenUrlClientTest::~RemoteOpenUrlClientTest() = default;

void RemoteOpenUrlClientTest::BindMockRemoteUrlOpener() {
  EXPECT_CALL(*api_provider_, GetSessionServices())
      .WillRepeatedly(Return(&mock_api_));
  EXPECT_CALL(mock_api_, BindRemoteUrlOpener(_))
      .WillRepeatedly(
          [this](mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver) {
            remote_url_opener_receiver_.Bind(std::move(receiver));
          });
}

TEST_F(RemoteOpenUrlClientTest, OpenFallbackBrowserWithNoUrl) {
  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(GURL(""))).Times(1);

  client_->OpenFallbackBrowser();
}

TEST_F(RemoteOpenUrlClientTest, OpenInvalidUrl_ShowsError) {
  EXPECT_CALL(*delegate_, ShowOpenUrlError(GURL("invalid-url"))).Times(1);
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run()).Times(1);

  client_->Open(ToCommandLineString("invalid-url"), done.Get());
}

TEST_F(RemoteOpenUrlClientTest, OpenUrlWithUnsupportedScheme_FallsBack) {
  EXPECT_CALL(*delegate_,
              OpenUrlOnFallbackBrowser(GURL("ftp://unsupported.com/")))
      .Times(1);
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run()).Times(1);

  client_->Open(ToCommandLineString("ftp://unsupported.com/"), done.Get());
}

TEST_F(RemoteOpenUrlClientTest,
       OpenWhenHostServicesApiIsNotProvided_FallsBack) {
  EXPECT_CALL(*api_provider_, GetSessionServices()).WillOnce(Return(nullptr));
  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(GURL("http://google.com/")))
      .Times(1);
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run()).Times(1);

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
}

TEST_F(RemoteOpenUrlClientTest, OpenUrlThenReceiverClosed_FallsBack) {
  BindMockRemoteUrlOpener();

  base::RunLoop run_loop;
  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(GURL("http://google.com/")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run()).Times(1);

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
  remote_url_opener_receiver_.reset();
  run_loop.Run();
}

TEST_F(RemoteOpenUrlClientTest, OpenUrl_Success) {
  BindMockRemoteUrlOpener();

  EXPECT_CALL(remote_url_opener_, OpenUrl(GURL("http://google.com/"), _))
      .WillOnce(base::test::RunOnceCallback<1>(mojom::OpenUrlResult::SUCCESS));

  base::RunLoop test_run_loop;
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run())
      .WillOnce(base::test::RunOnceClosure(test_run_loop.QuitClosure()));

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
  test_run_loop.Run();
}

TEST_F(RemoteOpenUrlClientTest, OpenUrl_Failure) {
  BindMockRemoteUrlOpener();

  EXPECT_CALL(remote_url_opener_, OpenUrl(GURL("http://google.com/"), _))
      .WillOnce(base::test::RunOnceCallback<1>(mojom::OpenUrlResult::FAILURE));
  EXPECT_CALL(*delegate_, ShowOpenUrlError(GURL("http://google.com/")))
      .Times(1);

  base::RunLoop test_run_loop;
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run())
      .WillOnce(base::test::RunOnceClosure(test_run_loop.QuitClosure()));

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
  test_run_loop.Run();
}

TEST_F(RemoteOpenUrlClientTest, OpenUrl_LocalFallback) {
  BindMockRemoteUrlOpener();

  EXPECT_CALL(remote_url_opener_, OpenUrl(GURL("http://google.com/"), _))
      .WillOnce(
          base::test::RunOnceCallback<1>(mojom::OpenUrlResult::LOCAL_FALLBACK));
  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(GURL("http://google.com/")))
      .Times(1);

  base::RunLoop test_run_loop;
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run())
      .WillOnce(base::test::RunOnceClosure(test_run_loop.QuitClosure()));

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
  test_run_loop.Run();
}

TEST_F(RemoteOpenUrlClientTest, OpenUrlTimeout_LocalFallback) {
  BindMockRemoteUrlOpener();

  mojom::RemoteUrlOpener::OpenUrlCallback captured_callback;
  EXPECT_CALL(remote_url_opener_, OpenUrl(GURL("http://google.com/"), _))
      .WillOnce([&](const GURL& url,
                    mojom::RemoteUrlOpener::OpenUrlCallback callback) {
        captured_callback = std::move(callback);
      });
  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(GURL("http://google.com/")))
      .Times(1);

  base::RunLoop test_run_loop;
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run())
      .WillOnce(base::test::RunOnceClosure(test_run_loop.QuitClosure()));

  client_->Open(ToCommandLineString("http://google.com/"), done.Get());
  test_run_loop.Run();

  // OpenUrlCallback fails a DCHECK if the callback is destroyed before it gets
  // called, so we have to capture it and call it here before it goes out of the
  // scope.
  std::move(captured_callback).Run(mojom::OpenUrlResult::FAILURE);
}

TEST_F(RemoteOpenUrlClientTest, OpenFilePath_LocalFallback) {
#if BUILDFLAG(IS_WIN)
  const wchar_t* file_path = L"C:\\test\\file\\path";
  GURL file_url("file:///C:/test/file/path");
#else
  const char* file_path = "/test/file/path";
  GURL file_url("file:///test/file/path");
#endif

  BindMockRemoteUrlOpener();

  EXPECT_CALL(*delegate_, OpenUrlOnFallbackBrowser(file_url)).Times(1);

  base::RunLoop test_run_loop;
  base::MockCallback<base::OnceClosure> done;
  EXPECT_CALL(done, Run())
      .WillOnce(base::test::RunOnceClosure(test_run_loop.QuitClosure()));

  client_->Open(file_path, done.Get());
  test_run_loop.Run();
}

}  // namespace remoting
