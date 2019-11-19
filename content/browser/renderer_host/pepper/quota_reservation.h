// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_QUOTA_RESERVATION_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_QUOTA_RESERVATION_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_stdint.h"  // For int64_t on Windows.
#include "ppapi/shared_impl/file_growth.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

namespace storage {
class FileSystemURL;
class OpenFileHandle;
class QuotaReservation;
}

namespace content {

struct QuotaReservationDeleter;

// This class holds a QuotaReservation and manages OpenFileHandles for checking
// quota. It should be created, used, and destroyed on a FileSystemContext's
// default_file_task_runner() instance. This is a RefCountedThreadSafe object
// because it needs to be passed to the file thread and kept alive during
// potentially long-running quota operations.
class CONTENT_EXPORT QuotaReservation
    : public base::RefCountedThreadSafe<QuotaReservation,
                                        QuotaReservationDeleter> {
 public:
  // Static method to facilitate construction on the file task runner.
  static scoped_refptr<QuotaReservation> Create(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const GURL& origin_url,
      storage::FileSystemType file_system_type);

  // Opens a file with the given id and path and returns its current size.
  int64_t OpenFile(int32_t id, const storage::FileSystemURL& url);
  // Closes the file opened by OpenFile with the given id.
  void CloseFile(int32_t id, const ppapi::FileGrowth& file_growth);
  // Refreshes the quota reservation to a new amount. A map that associates file
  // ids with maximum written offsets is provided as input. The callback will
  // receive a similar map with the updated file sizes.
  typedef base::Callback<void(int64_t, const ppapi::FileSizeMap&)>
      ReserveQuotaCallback;
  void ReserveQuota(int64_t amount,
                    const ppapi::FileGrowthMap& file_growth,
                    const ReserveQuotaCallback& callback);

  // Notifies underlying QuotaReservation that the associated client crashed,
  // and that the reserved quota is no longer traceable.
  void OnClientCrash();

 private:
  friend class base::RefCountedThreadSafe<QuotaReservation,
                                          QuotaReservationDeleter>;
  friend class base::DeleteHelper<QuotaReservation>;
  friend struct QuotaReservationDeleter;
  friend class QuotaReservationTest;

  QuotaReservation(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const GURL& origin_url,
      storage::FileSystemType file_system_type);

  // For unit testing only. A QuotaReservation intended for unit testing will
  // have file_system_context_ == NULL.
  QuotaReservation(scoped_refptr<storage::QuotaReservation> quota_reservation,
                   const GURL& origin_url,
                   storage::FileSystemType file_system_type);

  ~QuotaReservation();

  void GotReservedQuota(const ReserveQuotaCallback& callback,
                        base::File::Error error);

  void DeleteOnCorrectThread() const;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<storage::QuotaReservation> quota_reservation_;
  typedef std::map<int32_t, storage::OpenFileHandle*> FileMap;
  FileMap files_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservation);
};

struct QuotaReservationDeleter {
  static void Destruct(const QuotaReservation* quota_reservation) {
    quota_reservation->DeleteOnCorrectThread();
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_QUOTA_RESERVATION_H_
