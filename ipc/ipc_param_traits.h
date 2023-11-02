// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PARAM_TRAITS_H_
#define IPC_IPC_PARAM_TRAITS_H_

// Our IPC system uses the following partially specialized header to define how
// a data type is read, written and logged in the IPC system.

namespace IPC {
namespace internal {

template <typename T>
struct AlwaysFalse {
  static const bool value = false;
};

}  // namespace internal

template <class P> struct ParamTraits {
  static_assert(internal::AlwaysFalse<P>::value,
                "Cannot find the IPC::ParamTraits specialization. Did you "
                "forget to include the corresponding header file?");
};

template <class P>
struct SimilarTypeTraits {
  typedef P Type;
};

}  // namespace IPC

#endif  // IPC_IPC_PARAM_TRAITS_H_
