// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/mojo/test/js_test_interface.h"

#include "base/test/bind.h"

namespace ax {

JSTestInterface::JSTestInterface(base::OnceCallback<void(bool)> on_complete)
    : on_complete_(std::move(on_complete)),
      on_checkpoint_reached_(),
      receiver_(this) {}

JSTestInterface::JSTestInterface(
    base::OnceCallback<void(bool)> on_complete,
    base::RepeatingCallback<void(const std::string&)> on_checkpoint_reached)
    : on_complete_(std::move(on_complete)),
      on_checkpoint_reached_(std::move(on_checkpoint_reached)),
      receiver_(this) {}

JSTestInterface::~JSTestInterface() = default;
void JSTestInterface::BindReceiver(
    mojo::GenericPendingReceiver pending_receiver) {
  auto receiver = pending_receiver.As<axtest::mojom::TestBindingInterface>();
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindLambdaForTesting(
      [this]() { std::move(on_complete_).Run(false); }));
}

bool JSTestInterface::MatchesInterface(const std::string& interface_name) {
  return interface_name == "axtest.mojom.TestBindingInterface";
}

void JSTestInterface::AddTestInterface(AddTestInterfaceCallback callback) {
  int index = test_interface_receivers_.size();
  test_interface_receivers_.emplace_back();
  auto pending_receiver =
      test_interface_receivers_[index].BindNewPipeAndPassReceiver();
  std::move(callback).Run(std::move(pending_receiver));
}

void JSTestInterface::GetTestStruct(int num,
                                    const std::string& name,
                                    GetTestStructCallback callback) {
  auto result = axtest::mojom::TestStruct::New();
  result->is_structy = true;
  // Modify the passed values a bit to ensure it's not just an echo.
  result->num = num + 1;
  result->name = name + " rocks";
  std::move(callback).Run(std::move(result));
}

void JSTestInterface::SendEnumToTestInterface(axtest::mojom::TestEnum num) {
  for (auto& receiver : test_interface_receivers_) {
    receiver->TestMethod(num);
  }
}

void JSTestInterface::Disconnect() {
  receiver_.reset();
}

void JSTestInterface::TestComplete(bool success) {
  std::move(on_complete_).Run(success);
}

void JSTestInterface::CheckpointReached(
    const std::string& checkpoint_identifier) {
  if (!on_checkpoint_reached_.is_null()) {
    on_checkpoint_reached_.Run(checkpoint_identifier);
  }
}

void JSTestInterface::Log(const std::string& log_string) {
  LOG(INFO) << log_string;
}

}  // namespace ax
