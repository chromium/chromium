// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_TEST_NATIVE_TYPES_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_TEST_NATIVE_TYPES_H_

#include <string>

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace test {

class TestNativeStruct {
 public:
  TestNativeStruct();
  TestNativeStruct(const std::string& message, int x, int y);
  ~TestNativeStruct();

  const std::string& message() const { return message_; }
  void set_message(const std::string& message) { message_ = message; }

  int x() const { return x_; }
  void set_x(int x) { x_ = x; }

  int y() const { return y_; }
  void set_y(int y) { y_ = y; }

 private:
  std::string message_;
  int x_, y_;
};

class TestNativeStructWithAttachments {
 public:
  TestNativeStructWithAttachments();
  TestNativeStructWithAttachments(TestNativeStructWithAttachments&& other);
  TestNativeStructWithAttachments(const std::string& message,
                                  ScopedMessagePipeHandle pipe);

  TestNativeStructWithAttachments(const TestNativeStructWithAttachments&) =
      delete;
  TestNativeStructWithAttachments& operator=(
      const TestNativeStructWithAttachments&) = delete;

  ~TestNativeStructWithAttachments();

  TestNativeStructWithAttachments& operator=(
      TestNativeStructWithAttachments&& other);

  const std::string& message() const { return message_; }
  void set_message(const std::string& message) { message_ = message; }

  void set_pipe(mojo::ScopedMessagePipeHandle pipe) { pipe_ = std::move(pipe); }
  mojo::ScopedMessagePipeHandle PassPipe() const { return std::move(pipe_); }

 private:
  std::string message_;
  mutable mojo::ScopedMessagePipeHandle pipe_;
};

}  // namespace test
}  // namespace mojo

namespace IPC {

template <>
struct ParamTraits<mojo::test::TestNativeStruct> {
  using param_type = mojo::test::TestNativeStruct;

  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<mojo::test::TestNativeStructWithAttachments> {
  using param_type = mojo::test::TestNativeStructWithAttachments;

  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_TEST_NATIVE_TYPES_H_
