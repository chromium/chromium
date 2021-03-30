// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_CONTENT_RESTRICTION_H_
#define PDF_CONTENT_RESTRICTION_H_

namespace chrome_pdf {

// Used for disabling browser commands because of restrictions on how the data
// is to be used (i.e. can't copy/print).
// TODO(crbug.com/702993): Must be kept in sync with `ContentRestriction` in
// chrome/common/content_restriction.h. While there's a transitive static
// assertion that the enums match, a direct static assertion should be added
// when `PP_ContentRestriction` is removed.
enum ContentRestriction {
  kContentRestrictionCopy = 1 << 0,
  kContentRestrictionCut = 1 << 1,
  kContentRestrictionPaste = 1 << 2,
  kContentRestrictionPrint = 1 << 3,
  kContentRestrictionSave = 1 << 4
};

}  // namespace chrome_pdf

#endif  // PDF_CONTENT_RESTRICTION_H_
