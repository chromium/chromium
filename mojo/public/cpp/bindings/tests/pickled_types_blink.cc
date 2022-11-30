// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/pickled_types_blink.h"

#include "base/check_op.h"
#include "base/pickle.h"

namespace mojo {
namespace test {

PickledStructBlink::PickledStructBlink() {}

PickledStructBlink::PickledStructBlink(int foo, int bar)
    : foo_(foo), bar_(bar) {
  DCHECK_GE(foo_, 0);
  DCHECK_GE(bar_, 0);
}

PickledStructBlink::~PickledStructBlink() {}

}  // namespace test
}  // namespace mojo

namespace IPC {

void ParamTraits<mojo::test::PickledStructBlink>::Write(base::Pickle* m,
                                                        const param_type& p) {
  m->WriteInt(p.foo());
  m->WriteInt(p.bar());
}

bool ParamTraits<mojo::test::PickledStructBlink>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  int foo, bar;
  if (!iter->ReadInt(&foo) || !iter->ReadInt(&bar) || foo < 0 || bar < 0)
    return false;

  p->set_foo(foo);
  p->set_bar(bar);
  return true;
}

#include "ipc/param_traits_write_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumBlink,
                          mojo::test::PickledEnumBlink::VALUE_1)
#include "ipc/param_traits_read_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumBlink,
                          mojo::test::PickledEnumBlink::VALUE_1)
#include "ipc/param_traits_log_macros.h"
IPC_ENUM_TRAITS_MAX_VALUE(mojo::test::PickledEnumBlink,
                          mojo::test::PickledEnumBlink::VALUE_1)

}  // namespace IPC
