// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_HISTOGRAMS_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_HISTOGRAMS_H_

#include "base/macros.h"

namespace content {

class AppCacheHistograms {
 public:
  static void CountReinitAttempt(bool repeated_attempt);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppCacheHistograms);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_HISTOGRAMS_H_
