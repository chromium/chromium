// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_
#define REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "remoting/base/result.h"
#include "remoting/proto/file_transfer.pb.h"

namespace IPC {

template <>
struct ParamTraits<remoting::protocol::FileTransfer_Error> {
  typedef remoting::protocol::FileTransfer_Error param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <typename SuccessType, typename ErrorType>
struct ParamTraits<remoting::Result<SuccessType, ErrorType>> {
  typedef remoting::Result<SuccessType, ErrorType> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // REMOTING_HOST_CHROMOTING_PARAM_TRAITS_H_
