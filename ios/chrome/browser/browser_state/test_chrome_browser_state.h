// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/net/net_types.h"

namespace sync_preferences {
class PrefServiceSyncable;
class TestingPrefServiceSyncable;
}

// This class is the implementation of ChromeBrowserState used for testing.
class TestChromeBrowserState : public ios::ChromeBrowserState {
 public:
  typedef std::vector<
      std::pair<BrowserStateKeyedServiceFactory*,
                BrowserStateKeyedServiceFactory::TestingFactory>>
      TestingFactories;

  typedef std::vector<
      std::pair<RefcountedBrowserStateKeyedServiceFactory*,
                RefcountedBrowserStateKeyedServiceFactory::TestingFactory>>
      RefcountedTestingFactories;

  ~TestChromeBrowserState() override;

  // BrowserState:
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;

  // ChromeBrowserState:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  ios::ChromeBrowserState* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ios::ChromeBrowserState* GetOffTheRecordChromeBrowserState() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  PrefService* GetPrefs() override;
  PrefService* GetOffTheRecordPrefs() override;
  ChromeBrowserStateIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;

  // This method is defined as empty following the paradigm of
  // TestingProfile::DestroyOffTheRecordProfile().
  void DestroyOffTheRecordChromeBrowserState() override {}

  // Creates a WebDataService. If not invoked, the web data service is null.
  void CreateWebDataService();

  // Creates the BookmkarBarModel. If not invoked the bookmark bar model is
  // NULL. If |delete_file| is true, the bookmarks file is deleted first, then
  // the model is created. As TestChromeBrowserState deletes the directory
  // containing the files used by HistoryService, the boolean only matters if
  // you're recreating the BookmarkModel.
  //
  // NOTE: this does not block until the bookmarks are loaded.
  void CreateBookmarkModel(bool delete_file);

  // !!!!!!!! WARNING: THIS IS GENERALLY NOT SAFE TO CALL! !!!!!!!!
  // Creates the history service. If |delete_file| is true, the history file is
  // deleted first, then the HistoryService is created. As
  // TestChromeBrowserState deletes the directory containing the files used by
  // HistoryService, this only matters if you're recreating the HistoryService.
  bool CreateHistoryService(bool delete_file) WARN_UNUSED_RESULT;

  // Returns the preferences as a TestingPrefServiceSyncable if possible or
  // null. Returns null for off-the-record TestChromeBrowserState and also
  // for TestChromeBrowserState initialized with a custom pref service.
  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefService();

  // Helper class that allows for parameterizing the building
  // of TestChromeBrowserStates.
  class Builder {
   public:
    Builder();
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

    // Creates the TestChromeBrowserState using previously-set settings.
    std::unique_ptr<TestChromeBrowserState> Build();

   private:
    // If true, Build() has been called.
    bool build_called_;

    // Various staging variables where values are held until Build() is invoked.
    base::FilePath state_path_;
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service_;

    TestingFactories testing_factories_;
    RefcountedTestingFactories refcounted_testing_factories_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

 protected:
  // Used to create the principal TestChromeBrowserState.
  TestChromeBrowserState(
      const base::FilePath& path,
      std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
      TestingFactories testing_factories,
      RefcountedTestingFactories refcounted_testing_factories);

 private:
  friend class Builder;

  // Used to create the incognito TestChromeBrowserState.
  explicit TestChromeBrowserState(
      TestChromeBrowserState* original_browser_state);

  // Initialization of the TestChromeBrowserState. This is a separate method
  // as it needs to be called after the bi-directional link between original
  // and off-the-record TestChromeBrowserState has been created.
  void Init();

  // We use a temporary directory to store testing browser state data.
  // This must be declared before anything that may make use of the
  // directory so as to ensure files are closed before cleanup.
  base::ScopedTempDir temp_dir_;

  // The path to this browser state.
  base::FilePath state_path_;

  // If non-null, |testing_prefs_| points to |prefs_|. It is there to avoid
  // casting as |prefs_| may not be a TestingPrefServiceSyncable.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  sync_preferences::TestingPrefServiceSyncable* testing_prefs_;

  // The incognito ChromeBrowserState instance that is associated with this
  // non-incognito ChromeBrowserState instance.
  std::unique_ptr<TestChromeBrowserState> otr_browser_state_;
  TestChromeBrowserState* original_browser_state_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeBrowserState);
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_TEST_CHROME_BROWSER_STATE_H_
