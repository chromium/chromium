// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "base/testfidl/cpp/fidl.h"
#include "mojo/public/cpp/base/fuchsia/example.mojom.h"
#include "mojo/public/cpp/base/fuchsia/test_interface_request_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fuchsia {

using base::testfidl::TestInterface;
using base::testfidl::TestInterfacePtr;

TEST(InterfaceRequestStructTraitsTest, Serialization) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestInterfacePtr test_ptr;
  fidl::InterfaceRequest<TestInterface> input_request = test_ptr.NewRequest();
  fidl::InterfaceRequest<TestInterface> output_request;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              fuchsia::test::mojom::TestInterfaceRequest>(input_request,
                                                          output_request));
  EXPECT_TRUE(output_request.is_valid());
}

}  // namespace fuchsia
