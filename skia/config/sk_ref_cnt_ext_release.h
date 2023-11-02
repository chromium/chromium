// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_CONFIG_SK_REF_CNT_EXT_RELEASE_H_
#define SKIA_CONFIG_SK_REF_CNT_EXT_RELEASE_H_

#ifdef SKIA_CONFIG_SK_REF_CNT_EXT_DEBUG_H_
#error Only one SkRefCnt should be used.
#endif

// Alternate implementation of SkRefCnt for Chromium release builds
class SK_API SkRefCnt : public SkRefCntBase {
public:
  void deref() const { SkRefCntBase::unref(); }
};

namespace WTF {
inline void Adopted(const SkRefCnt* object) {}
inline void RequireAdoption(const SkRefCnt* object) {}
}  // namespace WTF

using WTF::Adopted;
using WTF::RequireAdoption;

#endif  // SKIA_CONFIG_SK_REF_CNT_EXT_RELEASE_H_
