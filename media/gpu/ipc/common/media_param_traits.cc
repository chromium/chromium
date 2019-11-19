// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <sstream>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "media/gpu/ipc/common/media_param_traits.h"

namespace IPC {

void ParamTraits<media::BitstreamBuffer>::Write(base::Pickle* m,
                                                const param_type& p) {
  WriteParam(m, p.id());
  WriteParam(m, static_cast<uint64_t>(p.size()));
  DCHECK_GE(p.offset(), 0);
  WriteParam(m, static_cast<uint64_t>(p.offset()));
  WriteParam(m, p.presentation_timestamp());
  WriteParam(m, p.key_id());
  if (!p.key_id().empty()) {
    WriteParam(m, p.iv());
    WriteParam(m, p.subsamples());
  }
  WriteParam(m, p.DuplicateRegion());
}

bool ParamTraits<media::BitstreamBuffer>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  DCHECK(r);
  uint64_t size = 0;
  uint64_t offset = 0;
  if (!(ReadParam(m, iter, &r->id_) && ReadParam(m, iter, &size) &&
        ReadParam(m, iter, &offset) &&
        ReadParam(m, iter, &r->presentation_timestamp_) &&
        ReadParam(m, iter, &r->key_id_)))
    return false;

  base::CheckedNumeric<size_t> checked_size(size);
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid size: " << size;
    return false;
  }
  r->size_ = checked_size.ValueOrDie();

  base::CheckedNumeric<off_t> checked_offset(offset);
  if (!checked_offset.IsValid()) {
    DLOG(ERROR) << "Invalid offset: " << offset;
    return false;
  }
  r->offset_ = checked_offset.ValueOrDie();

  if (!r->key_id_.empty()) {
    if (!(ReadParam(m, iter, &r->iv_) && ReadParam(m, iter, &r->subsamples_)))
      return false;
  }

  return ReadParam(m, iter, &r->region_);
}

void ParamTraits<media::BitstreamBuffer>::Log(const param_type& p,
                                              std::string* l) {
  std::ostringstream oss;
  oss << "id=" << p.id() << ", size=" << p.size() << ", presentation_timestamp="
      << p.presentation_timestamp().ToInternalValue();
  l->append(oss.str());
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/gpu/ipc/common/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/gpu/ipc/common/media_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef MEDIA_GPU_IPC_COMMON_MEDIA_PARAM_TRAITS_MACROS_H_
#include "media/gpu/ipc/common/media_param_traits_macros.h"
}  // namespace IPC
