// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "media/base/ipc/media_param_traits_macros.h"

namespace media {
class AudioParameters;
}

namespace IPC {

template <>
struct ParamTraits<media::AudioParameters> {
  typedef media::AudioParameters param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<media::AudioParameters::HardwareCapabilities> {
  typedef media::AudioParameters::HardwareCapabilities param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_
