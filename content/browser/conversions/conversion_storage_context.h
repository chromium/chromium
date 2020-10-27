// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/browser/conversions/conversion_storage_delegate_impl.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace base {

class Clock;
class FilePath;

}  // namespace base

namespace content {

// Abstraction around a ConversionStorage instance which can be accessed on
// multiple sequences. Proxies calls to the SequencedTaskRunner running storage
// operations.
class CONTENT_EXPORT ConversionStorageContext
    : public base::RefCountedThreadSafe<ConversionStorageContext> {
 public:
  ConversionStorageContext(
      const base::FilePath& user_data_directory,
      std::unique_ptr<ConversionStorageDelegateImpl> delegate,
      const base::Clock* clock);

  // All of these methods proxy to the equivalent methods on |storage_|. All
  // callbacks are run on the sequence |this| is accessed on. Can be called from
  // any sequence.
  //
  // TODO(https://crbug.com/1066920): This class should use a
  // base::SequenceBound to encapsulate |storage_|. This would also allow us to
  // simply expose |storage_| to callers, rather than having to manually proxy
  // methods. This also avoids having to call PostTask manually and using
  // base::Unretained on |storage_|.
  void StoreImpression(const StorableImpression& impression);
  void MaybeCreateAndStoreConversionReports(
      const StorableConversion& conversion,
      base::OnceCallback<void(int)> callback);
  void GetConversionsToReport(
      base::Time max_report_time,
      base::OnceCallback<void(std::vector<ConversionReport>)> callback);
  void GetActiveImpressions(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback);
  void DeleteConversion(int64_t conversion_id,
                        base::OnceCallback<void(bool)> callback);
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 base::RepeatingCallback<bool(const url::Origin&)> filter,
                 base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<ConversionStorageContext>;
  ~ConversionStorageContext();

  // Task runner used to perform operations on |storage_|. Runs with
  // base::TaskPriority::BEST_EFFORT.
  scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;

  // ConversionStorage instance which is scoped to lifetime of
  // |storage_task_runner_|. |storage_| should be accessed by calling
  // base::PostTask with |storage_task_runner_|, and should not be accessed
  // directly. |storage_| will never be null.
  std::unique_ptr<ConversionStorage, base::OnTaskRunnerDeleter> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_CONTEXT_H_
