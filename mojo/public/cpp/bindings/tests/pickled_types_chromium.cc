// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/pickled_types_chromium.h"

#include "base/pickle.h"

namespace mojo {
namespace test {

PickledStructChromium::PickledStructChromium() {}

PickledStructChromium::PickledStructChromium(int foo, int bar)
    : foo_(foo), bar_(bar) {}

PickledStructChromium::~PickledStructChromium() {}

bool operator==(const PickledStructChromium& a,
                const PickledStructChromium& b) {
  return a.foo() == b.foo() && a.bar() == b.bar() && a.baz() == b.baz();
}

}  // namespace test
}  // namespace mojo

namespace IPC {

void ParamTraits<mojo::test::PickledStructChromium>::Write(
    base::Pickle* m,
    const param_type& p) {
  m->WriteInt(p.foo());
  m->WriteInt(p.bar());
}

bool ParamTraits<mojo::test::PickledStructChromium>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  int foo, bar;
  if (!iter->ReadInt(&foo) || !iter->ReadInt(&bar))
    return false;

  p->set_foo(foo);
  p->set_bar(bar);
  return true;
}

// Generate param trais read methods from macros.
#undef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_H_
#include "ipc/param_traits_read_macros.h"
#include "mojo/public/cpp/bindings/tests/pickled_types_chromium.h"

// Generate param traits write methods from macros.
#undef MOJO_PUBLIC_CPP_BINDINGS_TESTS_PICKLED_TYPES_CHROMIUM_H_
#include "ipc/param_traits_write_macros.h"
#include "mojo/public/cpp/bindings/tests/pickled_types_chromium.h"

}  // namespace IPC
