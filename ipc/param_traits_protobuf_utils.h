// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PARAM_TRAITS_PROTOBUF_UTILS_H_
#define IPC_PARAM_TRAITS_PROTOBUF_UTILS_H_

#include "base/pickle.h"
#include "build/build_config.h"
#include "ipc/param_traits.h"
#include "ipc/param_traits_utils.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace IPC {

template <class RepeatedFieldLike, class StorageType>
struct RepeatedFieldParamTraits {
  typedef RepeatedFieldLike param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.size());
    for (int i = 0; i < p.size(); i++) {
      WriteParam(m, p.Get(i));
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    size_t size;
    if (!iter->ReadLength(&size)) {
      return false;
    }
    // Avoid integer overflow / assertion failure in Reserve() function.
    if (size > INT_MAX / sizeof(StorageType)) {
      return false;
    }
    r->Reserve(size);
    for (size_t i = 0; i < size; i++) {
      if (!ReadParam(m, iter, r->Add())) {
        return false;
      }
    }
    return true;
  }
};

template <class P>
struct ParamTraits<google::protobuf::RepeatedField<P>>
    : RepeatedFieldParamTraits<google::protobuf::RepeatedField<P>, P> {};

template <class P>
struct ParamTraits<google::protobuf::RepeatedPtrField<P>>
    : RepeatedFieldParamTraits<google::protobuf::RepeatedPtrField<P>, void*> {};

}  // namespace IPC

#endif  // IPC_PARAM_TRAITS_PROTOBUF_UTILS_H_
