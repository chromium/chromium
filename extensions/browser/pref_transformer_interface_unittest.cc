// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_transformer_interface.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/pref_modifiability_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kTestPref[] = "test.pref";

class TestObserver : public PrefTransformerInterface::Observer {
 public:
  void OnPrefModifiabilityChanged(const std::string& browser_pref) override {
    notified_prefs_.push_back(browser_pref);
  }

  const std::vector<std::string>& notified_prefs() const {
    return notified_prefs_;
  }
  void Clear() { notified_prefs_.clear(); }

 private:
  std::vector<std::string> notified_prefs_;
};

class DefaultTransformer : public PrefTransformerInterface {
 public:
  std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override {
    return extension_pref.Clone();
  }
  std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override {
    return browser_pref.Clone();
  }
};

class FakeModifiabilityObserver : public PrefModifiabilityObserver {
 public:
  FakeModifiabilityObserver(PrefTransformerInterface* transformer,
                            content::BrowserContext* context,
                            base::OnceClosure on_destroy)
      : PrefModifiabilityObserver(transformer, context),
        on_destroy_(std::move(on_destroy)) {}

  ~FakeModifiabilityObserver() override { std::move(on_destroy_).Run(); }

  void TriggerSignal() { MaybeNotifyModifiabilityChanged(); }

 private:
  base::OnceClosure on_destroy_;
};

class ObservableTransformer : public DefaultTransformer {
 public:
  bool IsExtensionModifiable(content::BrowserContext* context) const override {
    auto it = modifiable_by_context_.find(context);
    return it == modifiable_by_context_.end() ? true : it->second;
  }

  void SetModifiable(content::BrowserContext* context, bool modifiable) {
    modifiable_by_context_[context] = modifiable;
  }

  void TriggerSignal(content::BrowserContext* context) {
    auto it = active_observers_.find(context);
    if (it != active_observers_.end()) {
      it->second->TriggerSignal();
    }
  }

  size_t live_modifiability_observers() const {
    return active_observers_.size();
  }

 protected:
  std::unique_ptr<PrefModifiabilityObserver> CreateModifiabilityObserver(
      content::BrowserContext* context) override {
    auto observer = std::make_unique<FakeModifiabilityObserver>(
        this, context,
        base::BindOnce(&ObservableTransformer::OnModifiabilityObserverDestroyed,
                       base::Unretained(this), context));
    active_observers_[context] = observer.get();
    return observer;
  }

 private:
  void OnModifiabilityObserverDestroyed(content::BrowserContext* context) {
    active_observers_.erase(context);
  }

  std::map<raw_ptr<content::BrowserContext>, bool> modifiable_by_context_;
  std::map<raw_ptr<content::BrowserContext>, raw_ptr<FakeModifiabilityObserver>>
      active_observers_;
};

class PrefTransformerInterfaceTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_a_;
  content::TestBrowserContext context_b_;
};

TEST_F(PrefTransformerInterfaceTest, DefaultBehaviorAndNoOpObservation) {
  DefaultTransformer transformer;
  EXPECT_TRUE(transformer.IsExtensionModifiable(&context_a_));

  TestObserver observer;
  transformer.AddObserver(&context_a_, kTestPref, &observer);
  transformer.RemoveObserver(&context_a_, &observer);
  EXPECT_THAT(observer.notified_prefs(), IsEmpty());
}

TEST_F(PrefTransformerInterfaceTest,
       ModifiabilityObserverLifecycleAndDeduplication) {
  ObservableTransformer transformer;
  TestObserver observer_1;
  TestObserver observer_2;
  TestObserver observer_other_context;

  EXPECT_EQ(0u, transformer.live_modifiability_observers());

  // Adding the first observer creates a `PrefModifiabilityObserver` for
  // `context_a_`; adding a second observer reuses it.
  transformer.AddObserver(&context_a_, kTestPref, &observer_1);
  EXPECT_EQ(1u, transformer.live_modifiability_observers());
  transformer.AddObserver(&context_a_, kTestPref, &observer_2);
  EXPECT_EQ(1u, transformer.live_modifiability_observers());

  transformer.AddObserver(&context_b_, kTestPref, &observer_other_context);
  EXPECT_EQ(2u, transformer.live_modifiability_observers());

  // Spurious signals that don't flip `IsExtensionModifiable()` are ignored.
  transformer.TriggerSignal(&context_a_);
  EXPECT_THAT(observer_1.notified_prefs(), IsEmpty());
  EXPECT_THAT(observer_2.notified_prefs(), IsEmpty());

  // Flipping modifiability for `context_a_` notifies only `context_a_`
  // observers.
  transformer.SetModifiable(&context_a_, false);
  transformer.TriggerSignal(&context_a_);
  EXPECT_THAT(observer_1.notified_prefs(), ElementsAre(kTestPref));
  EXPECT_THAT(observer_2.notified_prefs(), ElementsAre(kTestPref));
  EXPECT_THAT(observer_other_context.notified_prefs(), IsEmpty());

  // Repeated signal without another flip is deduplicated.
  observer_1.Clear();
  observer_2.Clear();
  transformer.TriggerSignal(&context_a_);
  EXPECT_THAT(observer_1.notified_prefs(), IsEmpty());

  // Removing one observer keeps monitoring active until the last observer for
  // `context_a_` is removed.
  transformer.RemoveObserver(&context_a_, &observer_1);
  EXPECT_EQ(2u, transformer.live_modifiability_observers());

  transformer.SetModifiable(&context_a_, true);
  transformer.TriggerSignal(&context_a_);
  EXPECT_THAT(observer_1.notified_prefs(), IsEmpty());
  EXPECT_THAT(observer_2.notified_prefs(), ElementsAre(kTestPref));

  transformer.RemoveObserver(&context_a_, &observer_2);
  EXPECT_EQ(1u, transformer.live_modifiability_observers());

  transformer.RemoveObserver(&context_b_, &observer_other_context);
  EXPECT_EQ(0u, transformer.live_modifiability_observers());
}

TEST_F(PrefTransformerInterfaceTest, RemoveObserverDuringNotification) {
  class SelfRemovingObserver : public PrefTransformerInterface::Observer {
   public:
    SelfRemovingObserver(ObservableTransformer* transformer,
                         content::BrowserContext* context)
        : transformer_(transformer), context_(context) {}

    void OnPrefModifiabilityChanged(const std::string& browser_pref) override {
      transformer_->RemoveObserver(context_, this);
      // Destruction of the `PrefModifiabilityObserver` is deferred until the
      // outermost notification loop completes.
      EXPECT_EQ(1u, transformer_->live_modifiability_observers());
      notified_prefs_.push_back(browser_pref);
    }

    const std::vector<std::string>& notified_prefs() const {
      return notified_prefs_;
    }

   private:
    raw_ptr<ObservableTransformer> transformer_;
    raw_ptr<content::BrowserContext> context_;
    std::vector<std::string> notified_prefs_;
  };

  ObservableTransformer transformer;
  SelfRemovingObserver self_removing(&transformer, &context_a_);
  transformer.AddObserver(&context_a_, kTestPref, &self_removing);
  EXPECT_EQ(1u, transformer.live_modifiability_observers());

  transformer.SetModifiable(&context_a_, false);
  transformer.TriggerSignal(&context_a_);
  EXPECT_EQ(0u, transformer.live_modifiability_observers());
  EXPECT_THAT(self_removing.notified_prefs(), ElementsAre(kTestPref));

  // Re-adding an observer after re-entrant removal starts fresh monitoring.
  TestObserver follow_up;
  transformer.AddObserver(&context_a_, kTestPref, &follow_up);
  EXPECT_EQ(1u, transformer.live_modifiability_observers());
  transformer.SetModifiable(&context_a_, true);
  transformer.TriggerSignal(&context_a_);
  EXPECT_THAT(follow_up.notified_prefs(), ElementsAre(kTestPref));
  transformer.RemoveObserver(&context_a_, &follow_up);
}

}  // namespace
}  // namespace extensions
