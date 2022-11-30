// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_BLINK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_BLINK_H_

#include <stddef.h>

#include <string>

#include "base/check_op.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace mojo {
namespace test {

// Implementation of types with IPC::ParamTraits for consumers in Blink.

enum class PickledEnumBlink { VALUE_0, VALUE_1 };

// To make things slightly more interesting, this variation of the type doesn't
// support negative values. It'll DCHECK if you try to construct it with any,
// and it will fail deserialization if negative values are decoded.
class PickledStructBlink {
 public:
  PickledStructBlink();
  PickledStructBlink(int foo, int bar);
  PickledStructBlink(PickledStructBlink&& other) = default;

  PickledStructBlink(const PickledStructBlink&) = delete;
  PickledStructBlink& operator=(const PickledStructBlink&) = delete;

  ~PickledStructBlink();

  PickledStructBlink& operator=(PickledStructBlink&& other) = default;

  int foo() const { return foo_; }
  void set_foo(int foo) {
    DCHECK_GE(foo, 0);
    foo_ = foo;
  }

  int bar() const { return bar_; }
  void set_bar(int bar) {
    DCHECK_GE(bar, 0);
    bar_ = bar;
  }

  // The |baz| field should never be serialized.
  int baz() const { return baz_; }
  void set_baz(int baz) { baz_ = baz; }

 private:
  int foo_ = 0;
  int bar_ = 0;
  int baz_ = 0;
};

}  // namespace test
}  // namespace mojo

namespace IPC {

template <>
struct ParamTraits<mojo::test::PickledStructBlink> {
  using param_type = mojo::test::PickledStructBlink;

  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l) {}
};

}  // namespace IPC

IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumBlink,
                          mojo::test::PickledEnumBlink::VALUE_1)

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_BLINK_H_
