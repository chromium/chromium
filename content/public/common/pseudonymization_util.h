// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PSEUDONYMIZATION_UTIL_H_
#define CONTENT_PUBLIC_COMMON_PSEUDONYMIZATION_UTIL_H_

#include <string_view>

#include "content/common/content_export.h"
#include "stdint.h"

// Forward-declarations of classes approved for using the PseudonymizationUtil.
//
// The declarations below might violate to some extent the layering (forcing us
// to name classes declared in higher-layers, above the //content layer).  This
// is okay, because:
// 1. These are only forward-declarations
// 2. The benefit of the forward-declarations (mechanically forcing privacy
//    review for new users of PseudonymizationUtil) seems to outweigh the
//    layering concerns.
// 3. There is a precedent for such layering-violating-friends-usage in
//    base::ScopedAllowBlocking.
namespace extensions {
class ExtensionIdForTracing;
}  // namespace extensions

namespace content {

class CONTENT_EXPORT PseudonymizationUtil {
 public:
  // This is a test only interface that is identical to the public interface,
  // but does not require friending.
  static uint32_t PseudonymizeStringForTesting(std::string_view string);

 private:
  // Pseudonymizes the input `string` by passing it through a one-way hash
  // function (e.g. SHA1) and salting with an pseudonymization salt (randomly
  // generated once per Chrome session and thrown away - never persisted or sent
  // to a server).
  //
  // The same input `string` value will be translated into the same
  // pseudonymized uint32_t value, as long as PseudonymizeString is called
  // within the same Chromium session.  This is true even across processes (e.g.
  // the same pseudonymization result will be produced in the Browser process
  // and Renderer processes).
  //
  // This method is thread-safe - it can be called on any thread.
  static uint32_t PseudonymizeString(std::string_view string);

  // NOTE: All usages of the PseudonymizationUtil class should be reviewed by
  // chrome-privacy-core@google.com (and when approved added to the friend list
  // below).
  friend class extensions::ExtensionIdForTracing;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PSEUDONYMIZATION_UTIL_H_
