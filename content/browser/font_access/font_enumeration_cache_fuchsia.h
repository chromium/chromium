// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FUCHSIA_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FUCHSIA_H_

#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Fuchsia implementation of FontEnumerationCache.
// TODO(crbug.com/1265639): Use fuchsia.font API when it provides enumeration.
class CONTENT_EXPORT FontEnumerationCacheFuchsia : public FontEnumerationCache {
 public:
  // The constructor is public for internal use of base::SequenceBound.
  //
  // Production code should call FontEnumerationCache::Create(). Testing code
  // should call FontEnumerationCache::CreateForTesting().
  FontEnumerationCacheFuchsia(absl::optional<std::string> locale_override,
                              base::PassKey<FontEnumerationCache>);

  FontEnumerationCacheFuchsia(const FontEnumerationCacheFuchsia&) = delete;
  FontEnumerationCacheFuchsia& operator=(const FontEnumerationCacheFuchsia&) =
      delete;

  ~FontEnumerationCacheFuchsia() override;

 protected:
  // FontEnumerationCache:
  blink::FontEnumerationTable ComputeFontEnumerationData(
      const std::string& locale) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_CACHE_FUCHSIA_H_
