// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/rust/tests/interop_tests/interop_test.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace interop_test {

TestServiceImpl::TestServiceImpl(
    mojo::PendingReceiver<mojom::TestService> receiver)
    : receiver_(this, std::move(receiver)) {}

TestServiceImpl::~TestServiceImpl() = default;

// This defines the corresponding method from the header, and has it simply
// pass the incoming value back along the pipe unchanged.
#define DEFINE_PASS_METHOD(MethodName, TypeName)  \
  void TestServiceImpl::Pass##MethodName(         \
      parser_unittests::mojom::TypeName##Ptr val, \
      Pass##MethodName##Callback callback) {      \
    std::move(callback).Run(std::move(val));      \
  }

DEFINE_PASS_METHOD(FourInts, FourInts)
DEFINE_PASS_METHOD(Nested, TwiceNested)
DEFINE_PASS_METHOD(Bools, TenBoolsAndTwoBytes)
DEFINE_PASS_METHOD(Enums, SomeEnums)
DEFINE_PASS_METHOD(Union, BaseUnion)
DEFINE_PASS_METHOD(NestedUnion, WithManyUnions)
DEFINE_PASS_METHOD(Arrays, Arrays)
DEFINE_PASS_METHOD(Maps, Maps)
DEFINE_PASS_METHOD(Strings, Strings)
DEFINE_PASS_METHOD(NullableBasics, NullableBasics)
DEFINE_PASS_METHOD(NullableOthers, NullableOthers)
DEFINE_PASS_METHOD(Handles, Handles)
DEFINE_PASS_METHOD(NestedHandles, NestedHandles)

std::unique_ptr<TestServiceImpl> GetTestService(uintptr_t& out_handle) {
  mojo::PendingRemote<mojom::TestService> remote;
  auto impl = std::make_unique<TestServiceImpl>(
      remote.InitWithNewPipeAndPassReceiver());
  out_handle = remote.PassPipe().release().value();
  return impl;
}

}  // namespace interop_test
