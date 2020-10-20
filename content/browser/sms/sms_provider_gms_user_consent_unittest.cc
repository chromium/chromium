// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/webotp_service.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/sms/sms_provider_gms_user_consent.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/content_unittests_jni_headers/SmsUserConsentFakes_jni.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using ::testing::_;
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

// SmsProviderGmsUserConsentTest tests the JNI bindings to the android
// SmsUserConsentReceiver and the handling of the SMS upon retrieval.
class SmsProviderGmsUserConsentTest : public RenderViewHostTestHarness {
 protected:
  SmsProviderGmsUserConsentTest() = default;
  ~SmsProviderGmsUserConsentTest() override = default;

  void SetUp() {
    RenderViewHostTestHarness::SetUp();
    provider_ = std::make_unique<SmsProviderGmsUserConsent>();
    j_fake_sms_retriever_client_.Reset(
        Java_FakeSmsUserConsentRetrieverClient_create(AttachCurrentThread()));
    Java_SmsUserConsentFakes_setUserConsentClientForTesting(
        AttachCurrentThread(), provider_->GetWebOTPServiceForTesting(),
        j_fake_sms_retriever_client_,
        ui::WindowAndroid::CreateForTesting()->GetJavaObject());
    provider_->AddObserver(&observer_);
  }

  void TriggerUserConsentSms(const std::string& sms) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsUserConsentRetrieverClient_triggerUserConsentSms(
        env, j_fake_sms_retriever_client_,
        base::android::ConvertUTF8ToJavaString(env, sms));
  }

  void TriggerTimeout() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_FakeSmsUserConsentRetrieverClient_triggerTimeout(
        env, j_fake_sms_retriever_client_);
  }

  SmsProviderGmsUserConsent* provider() { return provider_.get(); }

  NiceMock<MockObserver>* observer() { return &observer_; }

 private:
  std::unique_ptr<SmsProviderGmsUserConsent> provider_;
  NiceMock<MockObserver> observer_;
  base::android::ScopedJavaGlobalRef<jobject> j_fake_sms_retriever_client_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderGmsUserConsentTest);
};

}  // namespace

TEST_F(SmsProviderGmsUserConsentTest, Retrieve) {
  EXPECT_CALL(*observer(),
              OnReceive(Origin::Create(GURL("https://google.com")), "ABC123"));
  provider()->Retrieve(main_rfh());
  TriggerUserConsentSms("Hi\n@google.com #ABC123");
}

TEST_F(SmsProviderGmsUserConsentTest, IgnoreBadSms) {
  std::string test_url = "https://google.com";
  std::string good_sms = "Hi\n@google.com #ABC123";
  std::string bad_sms = "Hi\n@b.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

  provider()->Retrieve(main_rfh());
  TriggerUserConsentSms(bad_sms);
  TriggerUserConsentSms(good_sms);
}

TEST_F(SmsProviderGmsUserConsentTest, TaskTimedOut) {
  EXPECT_CALL(*observer(), OnReceive(_, _)).Times(0);
  provider()->Retrieve(main_rfh());
  TriggerTimeout();
}

TEST_F(SmsProviderGmsUserConsentTest, OneObserverTwoTasks) {
  std::string test_url = "https://google.com";

  EXPECT_CALL(*observer(), OnReceive(Origin::Create(GURL(test_url)), "ABC123"));

  // Two tasks for when 1 request gets aborted but the task is still triggered.
  provider()->Retrieve(main_rfh());
  provider()->Retrieve(main_rfh());

  // First timeout should be ignored.
  TriggerTimeout();
  TriggerUserConsentSms("Hi\n@google.com #ABC123");
}

}  // namespace content
