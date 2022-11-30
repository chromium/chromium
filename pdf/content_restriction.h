// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_CONTENT_RESTRICTION_H_
#define PDF_CONTENT_RESTRICTION_H_

namespace chrome_pdf {

// Used for disabling browser commands because of restrictions on how the data
// is to be used (i.e. can't copy/print). Must be kept in sync with
// `ContentRestriction` in chrome/common/content_restriction.h.
enum ContentRestriction {
  kContentRestrictionCopy = 1 << 0,
  kContentRestrictionCut = 1 << 1,
  kContentRestrictionPaste = 1 << 2,
  kContentRestrictionPrint = 1 << 3,
  kContentRestrictionSave = 1 << 4
};

}  // namespace chrome_pdf

#endif  // PDF_CONTENT_RESTRICTION_H_
