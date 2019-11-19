// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_

// Database interface used by DOMStorageArea. Abstracts the differences between
// the per-origin DOMStorageDatabases for localStorage and
// SessionStorageDatabase which stores multiple origins.

#include <string>

#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/common/content_export.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace content {

class CONTENT_EXPORT DOMStorageDatabaseAdapter {
 public:
  virtual ~DOMStorageDatabaseAdapter() {}
  virtual void ReadAllValues(DOMStorageValuesMap* result) = 0;
  virtual bool CommitChanges(
      bool clear_all_first, const DOMStorageValuesMap& changes) = 0;
  virtual void DeleteFiles() {}
  virtual void Reset() {}
  // Adds memory statistics to |pmd| object for tracing.
  virtual void ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                                 const std::string& name) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_DATABASE_ADAPTER_H_
