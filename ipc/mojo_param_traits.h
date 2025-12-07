// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_PARAM_TRAITS_H_
#define IPC_MOJO_PARAM_TRAITS_H_

#include "base/component_export.h"
#include "ipc/param_traits.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace IPC {

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<mojo::MessagePipeHandle> {
  typedef mojo::MessagePipeHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<mojo::DataPipeConsumerHandle> {
  typedef mojo::DataPipeConsumerHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

}  // namespace IPC

#endif  // IPC_MOJO_PARAM_TRAITS_H_
