// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/ranges.h"

namespace media {

template<>
void Ranges<base::TimeDelta>::DCheckLT(const base::TimeDelta& lhs,
                                       const base::TimeDelta& rhs) const {
  DCHECK(lhs < rhs) << lhs.ToInternalValue() << " < " << rhs.ToInternalValue();
}

}  // namespace media
