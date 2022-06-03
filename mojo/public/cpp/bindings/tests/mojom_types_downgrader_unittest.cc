// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "mojo/public/cpp/bindings/tests/module_to_downgrade.test-mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace mojom_types_downgrader_unittest {

class DowngradedTestInterfaceImpl : public mojom::DowngradedTestInterface {
 public:
  DowngradedTestInterfaceImpl() = default;
  ~DowngradedTestInterfaceImpl() override = default;

  void InterfaceRequest(mojom::FooRequest request) override {}
  void InterfacePtr(mojom::FooPtr ptr) override {}
  void AssociatedInterfacePtrInfo(
      mojom::FooAssociatedPtrInfo associated_ptr_info) override {}
  void AssociatedInterfaceRequest(
      mojom::FooAssociatedRequest associated_request) override {}
  void PendingReceiver(mojom::FooRequest receiver) override {}
  void PendingRemote(mojom::FooPtr remote) override {}
  void PendingAssociatedReceiver(
      mojom::FooAssociatedRequest associated_remote) override {}
  void PendingAssociatedRemote(
      mojom::FooAssociatedPtrInfo associated_receiver) override {}
  void MultipleParams2(mojom::FooPtr remote,
                       mojom::FooRequest receiver) override {}
  void MultipleParams3(mojom::FooPtr remote,
                       mojom::FooRequest receiver,
                       mojom::FooAssociatedPtrInfo associated_remote) override {
  }
  void MultipleParams4(
      mojom::FooPtr remote,
      mojom::FooRequest receiver,
      mojom::FooAssociatedPtrInfo associated_remote,
      mojom::FooAssociatedRequest associated_receiver) override {}
  void MethodWithResponseCallbackOneLine(
      mojom::FooPtr data,
      MethodWithResponseCallbackOneLineCallback callback) override {}
  void MethodWithResponseCallbackTwoLines(
      mojom::FooPtr data,
      MethodWithResponseCallbackTwoLinesCallback callback) override {}
  void OddSpaces(mojom::FooPtr remote, mojom::FooRequest receiver) override {}
  void OddSpacesAndLineBreak(mojom::FooPtr remote,
                             mojom::FooRequest receiver) override {}
};

using DowngradedInterfaceTest = testing::Test;

TEST_F(DowngradedInterfaceTest, DowngradedInterfaceCompiles) {
  // Simply check that this test compiles (it will fail to compile if the
  // mojom_types_downgrader.py script failed to produce the right output).
  DowngradedTestInterfaceImpl test;

  // Methods already declared with the old types in the mojom interface.
  base::OnceCallback<void(mojom::FooRequest)> foo_request_callback =
      base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                         DowngradedTestInterface::InterfaceRequest,
                     base::Unretained(&test));
  base::OnceCallback<void(mojom::FooPtr)> foo_ptr_callback =
      base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                         DowngradedTestInterface::InterfacePtr,
                     base::Unretained(&test));
  base::OnceCallback<void(mojom::FooAssociatedPtrInfo)>
      foo_associated_ptr_info_callback = base::BindOnce(
          &mojom_types_downgrader_unittest::mojom::DowngradedTestInterface::
              AssociatedInterfacePtrInfo,
          base::Unretained(&test));
  base::OnceCallback<void(mojom::FooAssociatedRequest)>
      foo_associated_request_callback = base::BindOnce(
          &mojom_types_downgrader_unittest::mojom::DowngradedTestInterface::
              AssociatedInterfaceRequest,
          base::Unretained(&test));

  // Methods declared with the new types in the mojom interface. Note that these
  // assignments won't compile unless the module_to_downgrade.test-mojom has
  // been properly downgraded via the mojom_types_downgrader.py script.
  base::OnceCallback<void(mojom::FooRequest)> foo_pending_receiver_callback =
      base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                         DowngradedTestInterface::PendingReceiver,
                     base::Unretained(&test));
  base::OnceCallback<void(mojom::FooPtr)> foo_pending_remote_callback =
      base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                         DowngradedTestInterface::PendingRemote,
                     base::Unretained(&test));
  base::OnceCallback<void(mojom::FooAssociatedRequest)>
      foo_pending_associated_receiver_callback =
          base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                             DowngradedTestInterface::PendingAssociatedReceiver,
                         base::Unretained(&test));
  base::OnceCallback<void(mojom::FooAssociatedPtrInfo)>
      foo_pending_associated_remote_callback =
          base::BindOnce(&mojom_types_downgrader_unittest::mojom::
                             DowngradedTestInterface::PendingAssociatedRemote,
                         base::Unretained(&test));
}

}  // namespace mojom_types_downgrader_unittest
}  // namespace test
}  // namespace mojo
