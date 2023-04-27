// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/net/net_types.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace sync_preferences {
class PrefServiceSyncable;
class TestingPrefServiceSyncable;
}  // namespace sync_preferences

namespace policy {
class UserCloudPolicyManager;
}

// This class is the implementation of ChromeBrowserState used for testing.
class TestChromeBrowserState final : public ChromeBrowserState {
 public:
  typedef std::vector<
      std::pair<BrowserStateKeyedServiceFactory*,
                BrowserStateKeyedServiceFactory::TestingFactory>>
      TestingFactories;

  typedef std::vector<
      std::pair<RefcountedBrowserStateKeyedServiceFactory*,
                RefcountedBrowserStateKeyedServiceFactory::TestingFactory>>
      RefcountedTestingFactories;

  TestChromeBrowserState(const TestChromeBrowserState&) = delete;
  TestChromeBrowserState& operator=(const TestChromeBrowserState&) = delete;

  ~TestChromeBrowserState() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;

  // ChromeBrowserState:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  ChromeBrowserState* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ChromeBrowserState* GetOffTheRecordChromeBrowserState() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  BrowserStatePolicyConnector* GetPolicyConnector() override;
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override;
  ChromeBrowserStateIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ChromeBrowserState> AsWeakPtr() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  void DestroyOffTheRecordChromeBrowserState() override;

  // Creates an off-the-record TestChromeBrowserState for
  // the current object, installing `testing_factories`
  // first.
  //
  // This is an error to call this method if the current
  // TestChromeBrowserState already has a off-the-record
  // object, or is itself off-the-record.
  //
  // This method will be called without factories if the
  // method `GetOffTheRecordBrowserState()` is called on
  // this object.
  TestChromeBrowserState* CreateOffTheRecordBrowserStateWithTestingFactories(
      TestingFactories testing_factories = {});

  // Returns the preferences as a TestingPrefServiceSyncable if possible or
  // null. Returns null for off-the-record TestChromeBrowserState and also
  // for TestChromeBrowserState initialized with a custom pref service.
  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefService();

  // Sets a SharedURLLoaderFactory for test.
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  // Helper class that allows for parameterizing the building
  // of TestChromeBrowserStates.
  class Builder {
   public:
    Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    // Adds a testing factory to the TestChromeBrowserState. These testing
    // factories are installed before the ProfileKeyedServices are created.
    void AddTestingFactory(
        BrowserStateKeyedServiceFactory* service_factory,
        BrowserStateKeyedServiceFactory::TestingFactory testing_factory);
    void AddTestingFactory(
        RefcountedBrowserStateKeyedServiceFactory* service_factory,
        RefcountedBrowserStateKeyedServiceFactory::TestingFactory
            testing_factory);

    // Sets the path to the directory to be used to hold ChromeBrowserState
    // data.
    void SetPath(const base::FilePath& path);

    // Sets the PrefService to be used by the ChromeBrowserState.
    void SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs);

    void SetPolicyConnector(
        std::unique_ptr<BrowserStatePolicyConnector> policy_connector);

    // Sets a UserCloudPolicyManager for test.
    void SetUserCloudPolicyManager(
        std::unique_ptr<policy::UserCloudPolicyManager>
            user_cloud_policy_manager);

    // Creates the TestChromeBrowserState using previously-set settings.
    std::unique_ptr<TestChromeBrowserState> Build();

   private:
    // If true, Build() has been called.
    bool build_called_;

    // Various staging variables where values are held until Build() is invoked.
    base::FilePath state_path_;
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service_;

    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
    std::unique_ptr<BrowserStatePolicyConnector> policy_connector_;

    TestingFactories testing_factories_;
    RefcountedTestingFactories refcounted_testing_factories_;
  };

 protected:
  // Used to create the principal TestChromeBrowserState.
  TestChromeBrowserState(
      const base::FilePath& path,
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
      TestingFactories testing_factories,
      RefcountedTestingFactories refcounted_testing_factories,
      std::unique_ptr<BrowserStatePolicyConnector> policy_connector,
      std::unique_ptr<policy::UserCloudPolicyManager>
          user_cloud_policy_manager);

 private:
  friend class Builder;

  // Used to create the incognito TestChromeBrowserState.
  TestChromeBrowserState(TestChromeBrowserState* original_browser_state,
                         TestingFactories testing_factories);

  // Initialization of the TestChromeBrowserState. This is a separate method
  // as it needs to be called after the bi-directional link between original
  // and off-the-record TestChromeBrowserState has been created.
  void Init();

  // The path to this browser state.
  base::FilePath state_path_;

  // If non-null, `testing_prefs_` points to `prefs_`. It is there to avoid
  // casting as `prefs_` may not be a TestingPrefServiceSyncable.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  sync_preferences::TestingPrefServiceSyncable* testing_prefs_;

  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<BrowserStatePolicyConnector> policy_connector_;

  // A SharedURLLoaderFactory for test.
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  // The incognito ChromeBrowserState instance that is associated with this
  // non-incognito ChromeBrowserState instance.
  std::unique_ptr<TestChromeBrowserState> otr_browser_state_;
  TestChromeBrowserState* original_browser_state_;

  base::WeakPtrFactory<TestChromeBrowserState> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_
