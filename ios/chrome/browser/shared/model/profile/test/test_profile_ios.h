// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_IOS_H_

#include <memory>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "ios/chrome/browser/net/model/net_types.h"
#include "ios/chrome/browser/policy/model/profile_policy_connector.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace sync_preferences {
class PrefServiceSyncable;
class TestingPrefServiceSyncable;
}  // namespace sync_preferences

namespace policy {
class UserCloudPolicyManager;
}

// This class is the implementation of ProfileIOS used for testing.
class TestProfileIOS final : public ProfileIOS {
 public:
  // Wrapper over std::variant to help type deduction when calling
  // AddTestingFactories(). See example call in the method's comment.
  struct TestingFactory {
    TestingFactory(
        ProfileKeyedServiceFactoryIOS* service_factory,
        ProfileKeyedServiceFactoryIOS::TestingFactory testing_factory);

    TestingFactory(RefcountedProfileKeyedServiceFactoryIOS* service_factory,
                   RefcountedProfileKeyedServiceFactoryIOS::TestingFactory
                       testing_factory);

    TestingFactory(TestingFactory&&);
    TestingFactory& operator=(TestingFactory&&);

    ~TestingFactory();

    std::variant<
        std::pair<ProfileKeyedServiceFactoryIOS*,
                  ProfileKeyedServiceFactoryIOS::TestingFactory>,
        std::pair<RefcountedProfileKeyedServiceFactoryIOS*,
                  RefcountedProfileKeyedServiceFactoryIOS::TestingFactory>>
        service_factory_and_testing_factory;
  };

  // Wrapper around std::vector to simplify the migration to OnceCallback
  // for *ProfileKeyedServiceFactoryIOS::*TestingFactory.
  class TestingFactories {
   public:
    TestingFactories();

    template <typename... Ts>
      requires(... && std::same_as<Ts, TestingFactory>)
    TestingFactories(Ts&&... ts) {
      (..., factories_.push_back(std::move(ts)));
    }

    TestingFactories(TestingFactories&&);
    TestingFactories& operator=(TestingFactories&&);

    ~TestingFactories();

    template <typename... Args>
    void emplace_back(Args&&... args) {
      factories_.emplace_back(std::forward<Args>(args)...);
    }

    using iterator = std::vector<TestingFactory>::iterator;
    iterator begin() { return factories_.begin(); }
    iterator end() { return factories_.end(); }

   private:
    std::vector<TestingFactory> factories_;
  };

  TestProfileIOS(const TestProfileIOS&) = delete;
  TestProfileIOS& operator=(const TestProfileIOS&) = delete;

  ~TestProfileIOS() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  const base::Uuid& GetWebKitStorageID() const override;

  // ProfileIOS:
  ProfileIOS* GetOriginalProfile() override;
  bool HasOffTheRecordProfile() const override;
  ProfileIOS* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  ProfilePolicyConnector* GetPolicyConnector() override;
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override;
  const sync_preferences::PrefServiceSyncable* GetSyncablePrefs()
      const override;
  ProfileIOSIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ProfileIOS> AsWeakPtr() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;

  // Creates an off-the-record TestProfileIOS for
  // the current object, installing `testing_factories`
  // first.
  //
  // This is an error to call this method if the current
  // TestProfileIOS already has a off-the-record
  // object, or is itself off-the-record.
  //
  // This method will be called without factories if the
  // method `GetOffTheRecordProfile()` is called on
  // this object.
  TestProfileIOS* CreateOffTheRecordProfileWithTestingFactories(
      TestingFactories testing_factories = {});

  // Returns the preferences as a TestingPrefServiceSyncable if possible or
  // null. Returns null for off-the-record TestProfileIOS and also
  // for TestProfileIOS initialized with a custom pref service.
  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefService();

  // Sets a SharedURLLoaderFactory for test.
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  // Helper class that allows for parameterizing the building
  // of TestProfileIOS.
  class Builder {
   public:
    Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    Builder(Builder&&);
    Builder& operator=(Builder&&);

    ~Builder();

    // Adds a testing factory to the TestProfileIOS. These testing factories
    // are installed before the Profile's KeyedServices are created.
    Builder& AddTestingFactory(
        ProfileKeyedServiceFactoryIOS* service_factory,
        ProfileKeyedServiceFactoryIOS::TestingFactory testing_factory);
    Builder& AddTestingFactory(
        RefcountedProfileKeyedServiceFactoryIOS* service_factory,
        RefcountedProfileKeyedServiceFactoryIOS::TestingFactory
            testing_factory);

    // Adds multiple testing factories to TestProfileIOS. These testing
    // factories are installed before the Profile's KeyedServices are created.
    // Example use:
    //
    // AddTestingFactories(
    //     {TestProfileIOS::TestingFactory{
    //          RegularServiceFactory::GetInstance(),
    //          RegularServiceFactory::GetDefaultFactory(),
    //      },
    //      TestProfileIOS::TestingFactory{
    //          RefcountedServiceFactory::GetInstance(),
    //          RefcountedServiceFactory::GetDefaultFactory(),
    //      }});
    Builder& AddTestingFactories(TestingFactories testing_factories);

    // Sets the name of the ProfileIOS. If not set, the profile will use an
    // arbitrary name.
    Builder& SetName(const std::string& name);

    // Sets the PrefService to be used by the ProfileIOS.
    Builder& SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs);

    Builder& SetPolicyConnector(
        std::unique_ptr<ProfilePolicyConnector> policy_connector);

    // Sets a UserCloudPolicyManager for test.
    Builder& SetUserCloudPolicyManager(
        std::unique_ptr<policy::UserCloudPolicyManager>
            user_cloud_policy_manager);

    // Sets the Webkit storage identifier for test.
    Builder& SetWebkitStorageId(const base::Uuid& webkit_storage_id);

    // Returns the name passed to `SetName()`, or if that was not called, an
    // arbitrary fallback value.
    std::string GetEffectiveName() const;

    // Creates the TestProfileIOS using previously-set settings.
    std::unique_ptr<TestProfileIOS> Build() &&;

    // Creates the TestProfileIOS using `data_dir` as base directory
    // for the storage, and other previously-set settings.
    std::unique_ptr<TestProfileIOS> Build(const base::FilePath& data_dir) &&;

   private:
    // Various staging variables where values are held until Build() is invoked.
    std::string profile_name_;
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service_;
    base::Uuid webkit_storage_id_;

    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
    std::unique_ptr<ProfilePolicyConnector> policy_connector_;

    TestingFactories testing_factories_;
  };

 private:
  friend class Builder;

  // Used to create the principal TestProfileIOS.
  TestProfileIOS(const base::FilePath& state_path,
                 std::string_view profile_name,
                 base::Uuid webkit_storage_id,
                 std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
                 TestingFactories testing_factories,
                 std::unique_ptr<ProfilePolicyConnector> policy_connector,
                 std::unique_ptr<policy::UserCloudPolicyManager>
                     user_cloud_policy_manager);

  // Used to create the incognito TestProfileIOS.
  TestProfileIOS(const base::FilePath& state_path,
                 TestProfileIOS* original_profile,
                 TestingFactories testing_factories);

  // Initialization of the TestProfileIOS. This is a separate method
  // as it needs to be called after the bi-directional link between original
  // and off-the-record TestProfileIOS has been created.
  void Init();

  // If non-null, `testing_prefs_` points to `prefs_`. It is there to avoid
  // casting as `prefs_` may not be a TestingPrefServiceSyncable.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      testing_prefs_;

  // The WebKit storage identifier. May be invalid.
  const base::Uuid webkit_storage_id_;

  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<ProfilePolicyConnector> policy_connector_;

  // A SharedURLLoaderFactory for test.
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  // The incognito ProfileIOS instance that is associated with this
  // non-incognito ProfileIOS instance.
  std::unique_ptr<TestProfileIOS> otr_profile_;
  raw_ptr<TestProfileIOS> original_profile_;

  base::WeakPtrFactory<TestProfileIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TEST_PROFILE_IOS_H_
