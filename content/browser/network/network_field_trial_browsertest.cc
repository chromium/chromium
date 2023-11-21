// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/mock_entropy_provider.h"
#include "content/browser/startup_helper.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kFieldTrialName[] = "UniqueFieldTrialName";
const char kFieldTrialGroup[] = "UniqueFieldTrialGroupName";

class TestFieldTrialListObserver : public base::FieldTrialList::Observer {
 public:
  TestFieldTrialListObserver() { base::FieldTrialList::AddObserver(this); }

  TestFieldTrialListObserver(const TestFieldTrialListObserver&) = delete;
  TestFieldTrialListObserver& operator=(const TestFieldTrialListObserver&) =
      delete;

  ~TestFieldTrialListObserver() override {
    base::FieldTrialList::RemoveObserver(this);
  }

  void WaitForTrialGroupToBeFinalized() { run_loop_.Run(); }

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override {
    if (trial.trial_name() == kFieldTrialName) {
      EXPECT_EQ(kFieldTrialGroup, group_name);
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class NetworkFieldTrialBrowserTest : public ContentBrowserTest {
 public:
  NetworkFieldTrialBrowserTest()
      : field_trial_list_(SetUpFieldTrialsAndFeatureList()) {
    // The field trial needs to be created before the NetworkService is created
    // so that the NetworkService will automatically be informed of its
    // existence.
    base::MockEntropyProvider entropy_provider;
    base::FieldTrial* trial = base::FieldTrialList::FactoryGetFieldTrial(
        kFieldTrialName, 1 /* total_probability */, kFieldTrialGroup,
        entropy_provider);
    EXPECT_TRUE(trial);
  }

  NetworkFieldTrialBrowserTest(const NetworkFieldTrialBrowserTest&) = delete;
  NetworkFieldTrialBrowserTest& operator=(const NetworkFieldTrialBrowserTest&) =
      delete;

  ~NetworkFieldTrialBrowserTest() override = default;

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

// Test that when the network process activates a field trial, the browser
// process is informed. See https://crbug.com/1018329
IN_PROC_BROWSER_TEST_F(NetworkFieldTrialBrowserTest, FieldTrialRegistered) {
  // The network::mojom::NetworkServiceTest interface is only available when the
  // network service is out of process.
  if (!IsOutOfProcessNetworkService())
    return;

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kFieldTrialName));

  // Tell the network service to activate the field trial and wait for it to
  // inform the browser process about it.
  TestFieldTrialListObserver observer;
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  network_service_test->ActivateFieldTrial(kFieldTrialName);
  observer.WaitForTrialGroupToBeFinalized();

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kFieldTrialName));
}

}  // namespace content
