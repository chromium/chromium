// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/test_with_profile.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/test_file_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_global_state.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Returns a callback that accept a single arguments and store it to `output`.
// The callback must not outlive the `output` parameter.
template <typename T>
base::OnceCallback<void(T)> CaptureArg(T& output) {
  return base::BindOnce([](T& output, T param) { output = std::move(param); },
                        std::ref(output));
}

}  // namespace

TestWithProfile::InitializedFeatureList::InitializedFeatureList(
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features) {
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

TestWithProfile::InitializedFeatureList::~InitializedFeatureList() = default;

TestWithProfile::TestWithProfile()
    : TestWithProfile(/*enabled_features=*/{}, /*disabled_features=*/{}) {}

TestWithProfile::TestWithProfile(
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features)
    : initialized_scoped_feature_list_(enabled_features, disabled_features),
      profile_data_dir_(base::CreateUniqueTempDirectoryScopedToTest()),
      profile_manager_(GetApplicationContext()->GetLocalState(),
                       profile_data_dir_) {
  TestingApplicationContext* application_context =
      TestingApplicationContext::GetGlobal();

  // IOSChromeIOThread needs to be created before the IO thread is started.
  // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
  // thread is then started after the creation of IOSChromeIOThread.
  chrome_io_ = std::make_unique<IOSChromeIOThread>(
      application_context->GetLocalState(), application_context->GetNetLog());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      application_context->GetSystemIdentityManager(), &profile_manager_,
      application_context->GetLocalState());

  // Register the objects with the TestingApplicationContext.
  application_context->SetIOSChromeIOThread(chrome_io_.get());
  application_context->SetProfileManagerAndAccountProfileMapper(
      &profile_manager_, account_profile_mapper_.get());

  application_context->GetOptimizationGuideGlobalState()
      ->prediction_model_store()
      .Initialize(base::CreateUniqueTempDirectoryScopedToTest());

  // Start the IO thread.
  web_task_environment_.StartIOThread();

  // Post a task to initialize the IOSChromeIOThread object on the IO thread.
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                base::Unretained(chrome_io_.get())));

  // Init the BrowserPolicyConnect as this is required to create an instance
  // of ProfileIOSImpl.
  application_context->GetBrowserPolicyConnector()->Init(
      application_context->GetLocalState(),
      application_context->GetSharedURLLoaderFactory());

  // IOSChromeIOThread requires the SystemURLRequestContextGetter() to be
  // created before the object is shutdown, so force its creation here.
  std::ignore = chrome_io_->system_url_request_context_getter();
}

TestWithProfile::~TestWithProfile() {
  TestingApplicationContext* application_context =
      TestingApplicationContext::GetGlobal();

  // The profiles must have been unloaded by this point. This is because
  // their KeyedService may depends on the AccountProfileMapper when the
  // AccountProfileMapper depends on the ProfileManagerIOS.
  profile_manager_.PrepareForDestruction();
  CHECK_EQ(profile_manager_.GetLoadedProfiles().size(), 0u);

  application_context->GetBrowserPolicyConnector()->Shutdown();
  application_context->GetIOSChromeIOThread()->NetworkTearDown();
  application_context->SetProfileManagerAndAccountProfileMapper(nullptr,
                                                                nullptr);
  application_context->SetIOSChromeIOThread(nullptr);
}

ScopedProfileKeepAliveIOS TestWithProfile::LoadProfile(
    std::string_view profile_name) {
  ScopedProfileKeepAliveIOS keep_alive;

  base::RunLoop run_loop;
  profile_manager_.LoadProfileAsync(
      profile_name, CaptureArg(keep_alive).Then(run_loop.QuitClosure()), {});
  run_loop.Run();

  return keep_alive;
}

ScopedProfileKeepAliveIOS TestWithProfile::CreateProfile(
    std::string_view profile_name) {
  ScopedProfileKeepAliveIOS keep_alive;

  base::RunLoop run_loop;
  profile_manager_.CreateProfileAsync(
      profile_name, CaptureArg(keep_alive).Then(run_loop.QuitClosure()), {});
  run_loop.Run();

  return keep_alive;
}
