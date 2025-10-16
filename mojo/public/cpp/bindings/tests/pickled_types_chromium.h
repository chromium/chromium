// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_H_

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_INTERNAL
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_INTERNAL

#include <stddef.h>

#include "ipc/param_traits.h"
#include "ipc/param_traits_macros.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace mojo {
namespace test {

// Implementation of types with IPC::ParamTraits for consumers in the greater
// Chromium tree.

enum class PickledEnumChromium { VALUE_0, VALUE_1, VALUE_2 };

class PickledStructChromium {
 public:
  PickledStructChromium();
  PickledStructChromium(int foo, int bar);
  PickledStructChromium(PickledStructChromium&& other) = default;

  PickledStructChromium(const PickledStructChromium&) = delete;
  PickledStructChromium& operator=(const PickledStructChromium&) = delete;

  ~PickledStructChromium();

  PickledStructChromium& operator=(PickledStructChromium&& other) = default;

  int foo() const { return foo_; }
  void set_foo(int foo) { foo_ = foo; }

  int bar() const { return bar_; }
  void set_bar(int bar) { bar_ = bar; }

  // The |baz| field should never be serialized.
  int baz() const { return baz_; }
  void set_baz(int baz) { baz_ = baz; }

 private:
  int foo_ = 0;
  int bar_ = 0;
  int baz_ = 0;
};

bool operator==(const PickledStructChromium& a, const PickledStructChromium& b);

}  // namespace test
}  // namespace mojo

namespace IPC {

template <>
struct ParamTraits<mojo::test::PickledStructChromium> {
  using param_type = mojo::test::PickledStructChromium;

  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

}  // namespace IPC

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_INTERNAL

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumChromium,
                          mojo::test::PickledEnumChromium::VALUE_2)

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_H_
