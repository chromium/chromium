// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "services/device/public/mojom/wake_lock.mojom-shared.h"

namespace extensions {

namespace {

// Args commonly passed to FakeWakeLockManager::CallFunction().
const char kDisplayArgs[] = "[\"display\"]";
const char kSystemArgs[] = "[\"system\"]";
const char kEmptyArgs[] = "[]";

// Different actions that can be performed as a result of a
// wake lock being activated or cancelled.
enum Request {
  BLOCK_APP_SUSPENSION,
  UNBLOCK_APP_SUSPENSION,
  BLOCK_DISPLAY_SLEEP,
  UNBLOCK_DISPLAY_SLEEP,
  // Returned by FakeWakeLockManager::PopFirstRequest() when no
  // requests are present.
  NONE,
};

// Tests instantiate this class to make PowerAPI's calls to simulate activate
// and cancel the wake locks and record the actions that would've been performed
// instead of actually blocking and unblocking power management.
class FakeWakeLockManager {
 public:
  explicit FakeWakeLockManager(content::BrowserContext* context)
      : browser_context_(context) {
    PowerAPI::Get(browser_context_)
        ->SetWakeLockFunctionsForTesting(
            base::BindRepeating(&FakeWakeLockManager::ActivateWakeLock,
                                base::Unretained(this)),
            base::BindRepeating(&FakeWakeLockManager::CancelWakeLock,
                                base::Unretained(this)));
  }

  FakeWakeLockManager(const FakeWakeLockManager&) = delete;
  FakeWakeLockManager& operator=(const FakeWakeLockManager&) = delete;

  ~FakeWakeLockManager() {
    PowerAPI::Get(browser_context_)
        ->SetWakeLockFunctionsForTesting(PowerAPI::ActivateWakeLockFunction(),
                                         PowerAPI::CancelWakeLockFunction());
  }

  // Removes and returns the first item from |requests_|.  Returns NONE if
  // |requests_| is empty.
  Request PopFirstRequest() {
    if (requests_.empty())
      return NONE;

    Request request = requests_.front();
    requests_.pop_front();
    return request;
  }

 private:
  // Activates a new fake wake lock with type |type|.
  void ActivateWakeLock(device::mojom::WakeLockType type) {
    if (is_active_) {
      if (type == type_)
        return;

      // Has an active wake lock already, perform ChangeType:
      switch (type) {
        case device::mojom::WakeLockType::kPreventAppSuspension:
          requests_.push_back(BLOCK_APP_SUSPENSION);
          requests_.push_back(UNBLOCK_DISPLAY_SLEEP);
          break;
        case device::mojom::WakeLockType::kPreventDisplaySleep:
          requests_.push_back(BLOCK_DISPLAY_SLEEP);
          requests_.push_back(UNBLOCK_APP_SUSPENSION);
          break;
        case device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
          NOTREACHED_IN_MIGRATION() << "Unexpected wake lock type " << type;
          break;
      }

      type_ = type;
      return;
    }

    // Wake lock is not active, so activate it:
    if (!is_active_) {
      switch (type) {
        case device::mojom::WakeLockType::kPreventAppSuspension:
          requests_.push_back(BLOCK_APP_SUSPENSION);
          break;
        case device::mojom::WakeLockType::kPreventDisplaySleep:
          requests_.push_back(BLOCK_DISPLAY_SLEEP);
          break;
        case device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
          NOTREACHED_IN_MIGRATION() << "Unexpected wake lock type " << type;
          break;
      }

      type_ = type;
      is_active_ = true;
    }
  }

  void CancelWakeLock() {
    if (!is_active_)
      return;
    switch (type_) {
      case device::mojom::WakeLockType::kPreventAppSuspension:
        requests_.push_back(UNBLOCK_APP_SUSPENSION);
        break;
      case device::mojom::WakeLockType::kPreventDisplaySleep:
        requests_.push_back(UNBLOCK_DISPLAY_SLEEP);
        break;
      case device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
        NOTREACHED_IN_MIGRATION() << "Unexpected wake lock type " << type_;
        break;
    }
    is_active_ = false;
  }

  raw_ptr<content::BrowserContext> browser_context_;

  device::mojom::WakeLockType type_;
  bool is_active_ = false;

  // Requests in chronological order.
  base::circular_deque<Request> requests_;
};

}  // namespace

class PowerAPITest : public ApiUnitTest {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();
    manager_ = std::make_unique<FakeWakeLockManager>(browser_context());
  }

  void TearDown() override {
    manager_.reset();
    ApiUnitTest::TearDown();
  }

 protected:
  // Shorthand for PowerRequestKeepAwakeFunction and
  // PowerReleaseKeepAwakeFunction.
  enum FunctionType {
    REQUEST,
    RELEASE,
  };

  // Calls the function described by |type| with |args|, a JSON list of
  // arguments, on behalf of |extension|.
  bool CallFunction(FunctionType type,
                    const std::string& args,
                    const extensions::Extension* extension) {
    scoped_refptr<ExtensionFunction> function;
    if (type == REQUEST) {
      function = base::MakeRefCounted<PowerRequestKeepAwakeFunction>();
    } else {
      function = base::MakeRefCounted<PowerReleaseKeepAwakeFunction>();
    }
    function->set_extension(extension);
    return api_test_utils::RunFunction(function.get(), args, browser_context());
  }

  // Send a notification to PowerAPI saying that |extension| has
  // been unloaded.
  void UnloadExtension(const extensions::Extension* extension) {
    PowerAPI::Get(browser_context())
        ->OnExtensionUnloaded(browser_context(), extension,
                              UnloadedExtensionReason::UNINSTALL);
  }

  std::unique_ptr<FakeWakeLockManager> manager_;
};

TEST_F(PowerAPITest, RequestAndRelease) {
  // Simulate an extension making and releasing a "display" request and a
  // "system" request.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerAPITest, RequestWithoutRelease) {
  // Simulate an extension calling requestKeepAwake() without calling
  // releaseKeepAwake().  The override should be automatically removed when
  // the extension is unloaded.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  UnloadExtension(extension());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerAPITest, ReleaseWithoutRequest) {
  // Simulate an extension calling releaseKeepAwake() without having
  // calling requestKeepAwake() earlier.  The call should be ignored.
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerAPITest, UpgradeRequest) {
  // Simulate an extension calling requestKeepAwake("system") and then
  // requestKeepAwake("display").  When the second call is made, a
  // display-sleep-blocking request should be made before the initial
  // app-suspension-blocking request is released.
  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerAPITest, DowngradeRequest) {
  // Simulate an extension calling requestKeepAwake("display") and then
  // requestKeepAwake("system").  When the second call is made, an
  // app-suspension-blocking request should be made before the initial
  // display-sleep-blocking request is released.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerAPITest, MultipleExtensions) {
  // Simulate an extension blocking the display from sleeping.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // Create a second extension that blocks system suspend.  No additional
  // wake lock is needed; the wake lock from the first extension
  // already covers the behavior requested by the second extension.
  scoped_refptr<const Extension> extension2(ExtensionBuilder("Test2").Build());
  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension2.get()));
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // When the first extension is unloaded, a new app-suspension wake lock
  // should be requested before the display-sleep wake lock is cancelled.
  UnloadExtension(extension());
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // Make the first extension request display-sleep wake lock again.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

}  // namespace extensions
