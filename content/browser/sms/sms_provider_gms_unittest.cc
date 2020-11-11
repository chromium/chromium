// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_provider_gms.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/content_unittests_jni_headers/SmsProviderFakes_jni.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using url::Origin;

namespace content {

namespace {

class MockObserver : public SmsProvider::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD2(OnReceive,
               bool(const Origin&, const std::string& one_time_code));
  MOCK_METHOD1(OnFailure, bool(SmsFetcher::FailureType));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

// SmsProviderGmsBaseTest tests the JNI bindings to the android provider, the
// handling of the SMS upon retrieval, and various failure scenarios.
// It creates and injects a fake sms retriver client to trigger various actions
// for testing purposes.
class SmsProviderGmsBaseTest : public RenderViewHostTestHarness {
 protected:
  SmsProviderGmsBaseTest() = default;
  virtual ~SmsProviderGmsBaseTest() override = default;

  void SetUp() {
    RenderViewHostTestHarness::SetUp();

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebOtpBackend, GetSwitch());

    test_window_ = ui::WindowAndroid::CreateForTesting();

    provider_ = std::make_unique<SmsProviderGms>();

    j_fake_sms_retriever_client_.Reset(
        Java_FakeSmsRetrieverClient_create(AttachCurrentThread()));

    provider_->SetClientAndWindowForTesting(j_fake_sms_retriever_client_,
                                            test_window_->GetJavaObject());

    provider_->AddObserver(&observer_);
  }

  void TearDown() {
    RenderViewHostTestHarness::TearDown();
    test_window_->Destroy(nullptr, nullptr);
  }

  void TriggerSms(const std::string& sms) {
    if (GetSwitch() == switches::kWebOtpBackendUserConsent) {
      TriggerSmsForUserConsent(sms);
    } else {
      TriggerSmsForVerification(sms);
    }
  }

  void TriggerSmsForUserConsent(const std::string& sms) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerUserConsentSms(
        env, j_fake_sms_retriever_client_,
        base::android::ConvertUTF8ToJavaString(env, sms));
  }

  void TriggerSmsForVerification(const std::string& sms) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerVerificationSms(
        env, j_fake_sms_retriever_client_,
        base::android::ConvertUTF8ToJavaString(env, sms));
  }

  void TriggerTimeout() {
    JNIEnv* env = base::android::AttachCurrentThread();
    if (GetSwitch() == switches::kWebOtpBackendUserConsent) {
      Java_FakeSmsRetrieverClient_triggerUserConsentTimeout(
          env, j_fake_sms_retriever_client_);
    } else {
      Java_FakeSmsRetrieverClient_triggerVerificationTimeout(
          env, j_fake_sms_retriever_client_);
    }
  }

  void TriggerUserDeniesPermission() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerUserDeniesPermission(
        env, j_fake_sms_retriever_client_, test_window_->GetJavaObject());
  }

  void TriggerUserGrantsPermission() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerUserGrantsPermission(
        env, j_fake_sms_retriever_client_, test_window_->GetJavaObject());
  }

  void TriggerAPIFailure(const std::string& failure_type) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsRetrieverClient_triggerFailure(
        env, j_fake_sms_retriever_client_,
        base::android::ConvertUTF8ToJavaString(env, failure_type));
  }

  SmsProviderGms* provider() { return provider_.get(); }

  NiceMock<MockObserver>* observer() { return &observer_; }

  virtual std::string GetSwitch() const = 0;

 private:
  std::unique_ptr<SmsProviderGms> provider_;
  NiceMock<MockObserver> observer_;
  base::android::ScopedJavaGlobalRef<jobject> j_fake_sms_retriever_client_;
  base::test::ScopedFeatureList feature_list_;
  ui::WindowAndroid* test_window_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderGmsBaseTest);
};

class SmsProviderGmsTest : public ::testing::WithParamInterface<std::string>,
                           public SmsProviderGmsBaseTest {
  std::string GetSwitch() const override { return GetParam(); }
};

// Fixture to be used with tests that are only applicable to the auto backend.
class SmsProviderGmsAutoTest : public SmsProviderGmsBaseTest {
  std::string GetSwitch() const override {
    return switches::kWebOtpBackendAuto;
  }
};

}  // namespace

TEST_P(SmsProviderGmsTest, Retrieve) {
  std::string test_url = "https://google.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));
  provider()->Retrieve(main_rfh());
  TriggerSms("Hi\n@google.com #ABC123");
}

TEST_P(SmsProviderGmsTest, IgnoreBadSms) {
  std::string test_url = "https://google.com";
  std::string good_sms = "Hi\n@google.com #ABC123";
  std::string bad_sms = "Hi\n@b.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

  provider()->Retrieve(main_rfh());
  TriggerSms(bad_sms);
  TriggerSms(good_sms);
}

TEST_P(SmsProviderGmsTest, TaskTimedOut) {
  EXPECT_CALL(*observer(), OnReceive(_, _)).Times(0);
  provider()->Retrieve(main_rfh());
  TriggerTimeout();
}

TEST_P(SmsProviderGmsTest, OneObserverTwoTasks) {
  std::string test_url = "https://google.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

  // Two tasks for when 1 request gets aborted but the task is still triggered.
  provider()->Retrieve(main_rfh());
  provider()->Retrieve(main_rfh());

  // First timeout should be ignored.
  TriggerTimeout();
  TriggerSms("Hi\n@google.com #ABC123");
}

// For common tests, instantiate the parametric tests three times:
// with user consent backend, with verification backend, and  with auto.
INSTANTIATE_TEST_SUITE_P(
    AllBackends,
    SmsProviderGmsTest,
    testing::Values(switches::kWebOtpBackendAuto,
                    switches::kWebOtpBackendSmsVerification,
                    switches::kWebOtpBackendUserConsent));

// These tests are only valid with auto backend.

TEST_F(SmsProviderGmsAutoTest, OneTimePermissionDeniedByUser) {
  EXPECT_CALL(*observer(), OnFailure(_)).Times(1);

  provider()->Retrieve(main_rfh());

  TriggerUserDeniesPermission();
}

TEST_F(SmsProviderGmsAutoTest, OneTimePermissionGrantedByUser) {
  std::string test_url = "https://example.com";
  EXPECT_CALL(*observer(), OnFailure(_)).Times(0);
  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

  provider()->Retrieve(main_rfh());

  TriggerUserGrantsPermission();
  TriggerSms("@example.com #ABC123 $50");
}

TEST_F(SmsProviderGmsAutoTest, OneTimePermissionNotGranted) {
  EXPECT_CALL(*observer(), OnFailure(_)).Times(1);

  provider()->Retrieve(main_rfh());

  TriggerAPIFailure("USER_PERMISSION_REQUIRED");
}

TEST_F(SmsProviderGmsAutoTest, ExpectedFailuresShouldFallback) {
  // These failures should not cancel the retrieve but should cause us to
  // fallback to the user consensus method.
  std::string test_url = "https://example.com";
  std::string good_sms = "Hi\n@example.com #ABC123";

  {
    EXPECT_CALL(*observer(), OnFailure(_)).Times(0);
    EXPECT_CALL(*observer(),
                OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

    TriggerAPIFailure("API_NOT_CONNECTED");
    TriggerSmsForUserConsent("Hi\n@example.com #ABC123");

    Mock::VerifyAndClearExpectations(observer());
  }

  {
    EXPECT_CALL(*observer(), OnFailure(_)).Times(0);
    EXPECT_CALL(*observer(),
                OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

    provider()->Retrieve(main_rfh());

    TriggerAPIFailure("PLATFORM_NOT_SUPPORTED");
    TriggerSmsForUserConsent("Hi\n@example.com #ABC123");

    Mock::VerifyAndClearExpectations(observer());
  }

  {
    EXPECT_CALL(*observer(), OnFailure(_)).Times(0);
    EXPECT_CALL(*observer(),
                OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

    provider()->Retrieve(main_rfh());

    TriggerAPIFailure("API_NOT_AVAILABLE");
    TriggerSmsForUserConsent("Hi\n@example.com #ABC123");

    Mock::VerifyAndClearExpectations(observer());
  }
}

}  // namespace content
