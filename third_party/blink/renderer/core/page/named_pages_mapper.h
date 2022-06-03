// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_NAMED_PAGES_MAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_NAMED_PAGES_MAPPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Mapper from page number to page name [1]. To be populated during paginated
// layout, and consulted when sending pages to the printing / PDF system. This
// can be used to provide page-specific properties, such as margins, size and
// orientation.
//
// A page name is represented by a string. Page names are case-sensitive. The
// initial 'page' value 'auto' is represented by an empty string.
//
// [1] https://www.w3.org/TR/css-page-3/#using-named-pages
class CORE_EXPORT NamedPagesMapper {
 public:
  // We start by inserting an unnamed ('auto') entry with indefinite page
  // count. In documents with no named pages at all, this is all we'll
  // get. Otherwise, subsequent calls to AddNamedPage() will terminate (or even
  // overwrite, if we add a named page at page index 0) the unnamed page run.
  NamedPagesMapper() { entries_.emplace_back(AtomicString()); }

  // Add an entry for a given page name. If the specified page index is lower
  // than the number of pages we already have, the entries after this will be
  // deleted.
  void AddNamedPage(const AtomicString& page_name, int page_index);

  // Give the first page a name. We normally name pages as we go through layout
  // and find breaks needed because of named pages, but if the first page has a
  // name, it means that no break is inserted there.
  void NameFirstPage(const AtomicString& page_name);

  const AtomicString& LastPageName() const { return entries_.back().page_name; }
  const AtomicString& NamedPageAtIndex(int page_index) const;

 private:
  // An entry of contiguous pages with the same name.
  struct Entry {
    explicit Entry(const AtomicString& page_name) : page_name(page_name) {}

    AtomicString page_name;

    // The last page that this entry applies for. -1 means that it applies to
    // all remaining pages. -1 is only allowed in the last entry.
    int last_page_index = -1;
  };

  Vector<Entry, 1> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_NAMED_PAGES_MAPPER_H_
