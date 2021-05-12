// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/conversion_storage_delegate_impl.h"
#include "content/common/content_export.h"

namespace base {

class Clock;
class FilePath;

}  // namespace base

namespace content {

// Thread-safe wrapper to a SequenceBound ConversionStorage instance. Created by
// the ConversionManagerImpl.
class CONTENT_EXPORT ConversionStorageContext
    : public base::RefCountedThreadSafe<ConversionStorageContext> {
 public:
  ConversionStorageContext(
      const base::FilePath& user_data_directory,
      std::unique_ptr<ConversionStorageDelegateImpl> delegate,
      const base::Clock* clock);

  const base::SequenceBound<ConversionStorage>& storage() const {
    return storage_;
  }

 private:
  friend class base::RefCountedThreadSafe<ConversionStorageContext>;
  ~ConversionStorageContext();

  base::SequenceBound<ConversionStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_
