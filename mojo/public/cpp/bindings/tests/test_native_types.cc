// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/test_native_types.h"

#include "ipc/mojo_param_traits.h"

namespace mojo {
namespace test {

TestNativeStruct::TestNativeStruct() = default;

TestNativeStruct::TestNativeStruct(const std::string& message, int x, int y)
    : message_(message), x_(x), y_(y) {}

TestNativeStruct::~TestNativeStruct() = default;

TestNativeStructWithAttachments::TestNativeStructWithAttachments() = default;

TestNativeStructWithAttachments::TestNativeStructWithAttachments(
    TestNativeStructWithAttachments&& other) = default;

TestNativeStructWithAttachments::TestNativeStructWithAttachments(
    const std::string& message,
    mojo::ScopedMessagePipeHandle pipe)
    : message_(message), pipe_(std::move(pipe)) {}

TestNativeStructWithAttachments::~TestNativeStructWithAttachments() = default;

TestNativeStructWithAttachments& TestNativeStructWithAttachments::operator=(
    TestNativeStructWithAttachments&& other) = default;

}  // namespace test
}  // namespace mojo

namespace IPC {

// static
void ParamTraits<mojo::test::TestNativeStruct>::Write(base::Pickle* m,
                                                      const param_type& p) {
  m->WriteString(p.message());
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

// static
bool ParamTraits<mojo::test::TestNativeStruct>::Read(const base::Pickle* m,
                                                     base::PickleIterator* iter,
                                                     param_type* r) {
  std::string message;
  if (!iter->ReadString(&message)) {
    return false;
  }
  int x, y;
  if (!iter->ReadInt(&x) || !iter->ReadInt(&y)) {
    return false;
  }
  r->set_message(message);
  r->set_x(x);
  r->set_y(y);
  return true;
}

// static
void ParamTraits<mojo::test::TestNativeStructWithAttachments>::Write(
    base::Pickle* m,
    const param_type& p) {
  m->WriteString(p.message());
  IPC::ParamTraits<mojo::MessagePipeHandle>::Write(m, p.PassPipe().release());
}

// static
bool ParamTraits<mojo::test::TestNativeStructWithAttachments>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  std::string message;
  if (!iter->ReadString(&message)) {
    return false;
  }
  r->set_message(message);

  mojo::MessagePipeHandle pipe;
  if (!IPC::ParamTraits<mojo::MessagePipeHandle>::Read(m, iter, &pipe)) {
    return false;
  }

  r->set_pipe(mojo::ScopedMessagePipeHandle(pipe));
  return true;
}

}  // namespace IPC
