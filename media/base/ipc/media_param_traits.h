// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_
#define MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_

#include "base/pickle.h"
#include "ipc/param_traits.h"
#include "media/base/ipc/media_param_traits_macros.h"

namespace media {
class AudioParameters;
class EncryptionPattern;
}

namespace IPC {

template <>
struct ParamTraits<media::AudioParameters> {
  typedef media::AudioParameters param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct ParamTraits<media::AudioParameters::HardwareCapabilities> {
  typedef media::AudioParameters::HardwareCapabilities param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct ParamTraits<media::EncryptionPattern> {
  typedef media::EncryptionPattern param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

}  // namespace IPC

#endif  // MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_H_
