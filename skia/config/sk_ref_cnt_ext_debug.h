// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_CONFIG_SK_REF_CNT_EXT_DEBUG_H_
#define SKIA_CONFIG_SK_REF_CNT_EXT_DEBUG_H_

#ifdef SKIA_CONFIG_SK_REF_CNT_EXT_RELEASE_H_
#error Only one SkRefCnt should be used.
#endif

#include <atomic>

class SkRefCnt;

namespace WTF {
  void adopted(const SkRefCnt*);
  void requireAdoption(const SkRefCnt*);
}

// Alternate implementation of SkRefCnt for Chromium debug builds
class SK_API SkRefCnt : public SkRefCntBase {
public:
  SkRefCnt();
  ~SkRefCnt() override;
  void ref() const { SkASSERT(flags_.load() != AdoptionRequired_Flag); SkRefCntBase::ref(); }
  void deref() const { SkRefCntBase::unref(); }
private:
  void adopted() const { flags_ |= Adopted_Flag; }
  void requireAdoption() const { flags_ |= AdoptionRequired_Flag; }

  enum {
    Adopted_Flag = 0x1,
    AdoptionRequired_Flag = 0x2,
  };

  mutable std::atomic<int> flags_;

  friend void WTF::adopted(const SkRefCnt*);
  friend void WTF::requireAdoption(const SkRefCnt*);
};

inline SkRefCnt::SkRefCnt() : flags_(0) { }

inline SkRefCnt::~SkRefCnt() { }

// Bootstrap for Blink's WTF::RefPtr

namespace WTF {
inline void adopted(const SkRefCnt* object) {
  if (!object)
    return;
  object->adopted();
}
inline void requireAdoption(const SkRefCnt* object) {
  if (!object)
    return;
  object->requireAdoption();
}
}  // namespace WTF

using WTF::adopted;
using WTF::requireAdoption;

#endif  // SKIA_CONFIG_SK_REF_CNT_EXT_DEBUG_H_
