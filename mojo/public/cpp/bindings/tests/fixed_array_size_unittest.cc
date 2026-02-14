// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The wire format encodes variable-sized (e.g. `array<int32>`) and fixed-size
// (e.g. `array<int32, 3>` arrays identically. These tests ensure that
// deserializing a fixed-size array with the wrong number of elements does not
// succeed, i.e. triggers a validation error.

#include <utility>

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/fixed_array_size_unittest.test-mojom.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/cpp/test_support/validation_errors_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::fixed_array_size_unittest {

class Interface2 : public mojom::Interface2 {
 public:
  explicit Interface2(ScopedMessagePipeHandle pipe)
      : receiver_(this,
                  mojo::PendingReceiver<mojom::Interface2>(std::move(pipe))) {}
  void Send(mojom::Struct2Ptr) override { NOTREACHED(); }

 private:
  mojo::Receiver<mojom::Interface2> receiver_;
};

class Interface3 : public mojom::Interface3 {
 public:
  explicit Interface3(ScopedMessagePipeHandle pipe)
      : receiver_(this,
                  mojo::PendingReceiver<mojom::Interface3>(std::move(pipe))) {}
  void Send(const TypemappedStruct3&) override { NOTREACHED(); }

 private:
  mojo::Receiver<mojom::Interface3> receiver_;
};

class Interface4 : public mojom::Interface4 {
 public:
  explicit Interface4(ScopedMessagePipeHandle pipe)
      : receiver_(this,
                  mojo::PendingReceiver<mojom::Interface4>(std::move(pipe))) {}
  void Send(mojom::Struct4Ptr) override { NOTREACHED(); }

 private:
  mojo::Receiver<mojom::Interface4> receiver_;
};

class FixedArraySizeTest : public ::testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FixedArraySizeTest, FixedToFixedNotEnough) {
  mojo::Remote<Interface2> remote;
  Interface4 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::Struct2Ptr input = mojom::Struct2::New();
  input->values = {1, 2};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, FixedToFixedTooMany) {
  mojo::Remote<Interface4> remote;
  Interface2 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::Struct4Ptr input = mojom::Struct4::New();
  input->values = {1, 2, 3, 4};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, VariableToFixedNotEnough) {
  mojo::Remote<mojom::InterfaceVariable> remote;
  Interface4 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::StructVariablePtr input = mojom::StructVariable::New();
  input->values = {1};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, VariableToFixedTooMany) {
  mojo::Remote<mojom::InterfaceVariable> remote;
  Interface2 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::StructVariablePtr input = mojom::StructVariable::New();
  input->values = {1, 2, 3, 4, 5};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, FixedToTypemappedNotEnough) {
  mojo::Remote<Interface2> remote;
  Interface3 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::Struct2Ptr input = mojom::Struct2::New();
  input->values = {1, 2};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, FixedToTypemappedTooMany) {
  mojo::Remote<Interface4> remote;
  Interface3 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::Struct4Ptr input = mojom::Struct4::New();
  input->values = {1, 2, 3, 4};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, VariableToTypemappedNotEnough) {
  mojo::Remote<mojom::InterfaceVariable> remote;
  Interface3 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::StructVariablePtr input = mojom::StructVariable::New();
  input->values = {1};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

TEST_F(FixedArraySizeTest, VariableToTypemappedTooMany) {
  mojo::Remote<mojom::InterfaceVariable> remote;
  Interface3 impl(remote.BindNewPipeAndPassReceiver().PassPipe());

  mojom::StructVariablePtr input = mojom::StructVariable::New();
  input->values = {1, 2, 3, 4, 5};
  remote->Send(std::move(input));

  base::RunLoop loop;
  mojo::internal::ValidationErrorObserverForTesting observer(
      loop.QuitClosure());
  loop.Run();
  EXPECT_EQ(mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER,
            observer.last_error());
}

}  // namespace mojo::test::fixed_array_size_unittest
