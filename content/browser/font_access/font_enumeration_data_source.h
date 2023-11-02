// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

// Retrieves the font list used by the Font Access API from the underlying OS.
//
// Implementations are not expected to be thread-safe. All methods except for
// the constructor must be used on the same sequence. The sequence must allow
// blocking I/O operations.
//
// Implementations do not currently store any state, so the class infrastructure
// seems like overkill. However, the Font Access API will soon report changes to
// the list of installed fonts. Detecting changes will require storing
// OS-specific data.
class CONTENT_EXPORT FontEnumerationDataSource {
 public:
  // Factory method that instantiates the correct per-OS implementation.
  //
  // The result is guaranteed to be non-null.
  static std::unique_ptr<FontEnumerationDataSource> Create();

  // Exposed for std::make_unique. Instances should be obtained from Create().
  FontEnumerationDataSource() = default;

  FontEnumerationDataSource(const FontEnumerationDataSource&) = delete;
  FontEnumerationDataSource& operator=(const FontEnumerationDataSource&) =
      delete;

  virtual ~FontEnumerationDataSource() = default;

  // Heavyweight method whose result should be cached.
  //
  // Must be called on a sequence where blocking I/O operations are allowed.
  virtual blink::FontEnumerationTable GetFonts(const std::string& locale) = 0;

  // True if we have an implementation for the underlying OS.
  //
  // Tests can expect that GetData() returns a non-empty list on supported
  // operating systems.
  static bool IsOsSupported();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ENUMERATION_DATA_SOURCE_H_
