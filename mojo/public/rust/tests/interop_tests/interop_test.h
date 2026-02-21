// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_TESTS_INTEROP_TESTS_INTEROP_TEST_H_
#define MOJO_PUBLIC_RUST_TESTS_INTEROP_TESTS_INTEROP_TEST_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/rust/mojom_value_parser/test_util/parser_unittests.test-mojom.h"
#include "mojo/public/rust/tests/interop_tests/interop_test.test-mojom.h"

// Things get samey real fast so let's use a macro
// This defines, e.g. `PassEnums(SomeEnumsPtr ...)`
#define DECLARE_PASS_METHOD(MethodName, TypeName)                   \
  void Pass##MethodName(parser_unittests::mojom::TypeName##Ptr val, \
                        Pass##MethodName##Callback callback) override;

namespace interop_test {

class TestServiceImpl : public mojom::TestService {
 public:
  explicit TestServiceImpl(mojo::PendingReceiver<mojom::TestService> receiver);
  ~TestServiceImpl() override;

  // mojom::TestService implementation:
  DECLARE_PASS_METHOD(FourInts, FourInts)
  DECLARE_PASS_METHOD(Nested, TwiceNested)
  DECLARE_PASS_METHOD(Bools, TenBoolsAndTwoBytes)
  DECLARE_PASS_METHOD(Enums, SomeEnums)
  DECLARE_PASS_METHOD(Union, BaseUnion)
  DECLARE_PASS_METHOD(NestedUnion, WithManyUnions)
  DECLARE_PASS_METHOD(Arrays, Arrays)
  DECLARE_PASS_METHOD(Maps, Maps)
  DECLARE_PASS_METHOD(Strings, Strings)
  DECLARE_PASS_METHOD(NullableBasics, NullableBasics)
  DECLARE_PASS_METHOD(NullableOthers, NullableOthers)
  DECLARE_PASS_METHOD(Handles, Handles)
  DECLARE_PASS_METHOD(NestedHandles, NestedHandles)

 private:
  mojo::Receiver<mojom::TestService> receiver_;
};

// Function to be called from Rust to get a remote to a C++ TestService.
// Returns a unique_ptr to the implementation, and passes the handle to the
// remote back via `out_handle`.
std::unique_ptr<TestServiceImpl> GetTestService(uintptr_t& out_handle);

}  // namespace interop_test

#endif  // MOJO_PUBLIC_RUST_TESTS_INTEROP_TESTS_INTEROP_TEST_H_
