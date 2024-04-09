
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_MOJO_TEST_JS_TEST_INTERFACE_H_
#define SERVICES_ACCESSIBILITY_FEATURES_MOJO_TEST_JS_TEST_INTERFACE_H_

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/features/interface_binder.h"
#include "services/accessibility/features/mojo/test/test_api.test-mojom.h"

namespace ax {

// C++ implementation of the TestBindingInterface mojom. The other end of the
// pipe is held in Javascript. This can be used to run tests in JS and inform
// when they fail or complete.
class JSTestInterface : public axtest::mojom::TestBindingInterface,
                        public InterfaceBinder {
 public:
  // Creates a JSTestInterface with a callback that runs when the test
  // has finished with a bool representing whether the test was successful.
  explicit JSTestInterface(base::OnceCallback<void(bool)> on_complete);
  JSTestInterface(
      base::OnceCallback<void(bool)> on_complete,
      base::RepeatingCallback<void(const std::string&)> on_checkpoint_reached);

  ~JSTestInterface() override;
  JSTestInterface& operator=(const JSTestInterface&) = delete;
  JSTestInterface(const JSTestInterface&) = delete;

  // InterfaceBinder overrides:
  void BindReceiver(mojo::GenericPendingReceiver pending_receiver) override;
  bool MatchesInterface(const std::string& interface_name) override;

  // axtest::mojom::TestBindingInterface overrides:
  void AddTestInterface(AddTestInterfaceCallback callback) override;
  void GetTestStruct(int num,
                     const std::string& name,
                     GetTestStructCallback callback) override;
  void SendEnumToTestInterface(axtest::mojom::TestEnum num) override;
  void Disconnect() override;
  void TestComplete(bool success) override;
  void Log(const std::string& log_string) override;
  void CheckpointReached(const std::string& checkpoint_identifier) override;

 private:
  base::OnceCallback<void(bool)> on_complete_;
  base::RepeatingCallback<void(const std::string&)> on_checkpoint_reached_;
  mojo::Receiver<axtest::mojom::TestBindingInterface> receiver_;
  std::vector<mojo::Remote<axtest::mojom::TestInterface>>
      test_interface_receivers_;
};
}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_MOJO_TEST_JS_TEST_INTERFACE_H_
