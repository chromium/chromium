// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRERENDER_PRERENDER_REL_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRERENDER_PRERENDER_REL_TYPE_H_

namespace blink {

// WebPrerenderRelType is a bitfield since multiple rel attributes can be set on
// the same prerender.
enum WebPrerenderRelType {
  kPrerenderRelTypePrerender = 0x1,
  kPrerenderRelTypeNext = 0x2,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRERENDER_PRERENDER_REL_TYPE_H_
