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

#include "ipc/param_traits_write_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumChromium,
                          mojo::test::PickledEnumChromium::VALUE_2)
#include "ipc/param_traits_read_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumChromium,
                          mojo::test::PickledEnumChromium::VALUE_2)
#include "ipc/param_traits_log_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumChromium,
                          mojo::test::PickledEnumChromium::VALUE_2)

}  // namespace IPC
