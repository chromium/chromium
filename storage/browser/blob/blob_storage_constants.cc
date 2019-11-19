// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_constants.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace storage {
namespace {
// Specifies the minimum file size.
constexpr const char kBlobFileTransportMinFileSizeSwitch[] =
    "blob-transport-file-min-size";
// Specifies the maximum file size.
constexpr const char kBlobFileTransportMaxFileSizeSwitch[] =
    "blob-transport-file-max-size";
// Specifies a custom maximum size of the shared memory segments used to
// transport blob.
const char kBlobSharedMemoryTransportMaxSizeSwitch[] =
    "blob-transport-shared-memory-max-size";
}  // namespace

static_assert(kDefaultIPCMemorySize < kDefaultSharedMemorySize,
              "IPC transport size must be smaller than shared memory size.");
static_assert(kDefaultMinPageFileSize < kDefaultMaxPageFileSize,
              "Min page file size must be less than max.");
static_assert(kDefaultMinPageFileSize < kDefaultMaxBlobInMemorySpace,
              "Page file size must be less than in-memory space.");

BlobStorageLimits::BlobStorageLimits() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (UNLIKELY(
          command_line->HasSwitch(kBlobFileTransportByFileTriggerSwitch))) {
    CHECK(base::StringToUint64(command_line->GetSwitchValueASCII(
                                   kBlobFileTransportByFileTriggerSwitch),
                               &override_file_transport_min_size))
        << "Unable to parse "
        << command_line->GetSwitchValueASCII(
               kBlobFileTransportByFileTriggerSwitch);
  }
  if (UNLIKELY(
          command_line->HasSwitch(kBlobSharedMemoryTransportMaxSizeSwitch))) {
    CHECK(base::StringToSizeT(command_line->GetSwitchValueASCII(
                                  kBlobSharedMemoryTransportMaxSizeSwitch),
                              &max_shared_memory_size))
        << "Unable to parse "
        << command_line->GetSwitchValueASCII(
               kBlobSharedMemoryTransportMaxSizeSwitch);
  }
  if (UNLIKELY(command_line->HasSwitch(kBlobFileTransportMinFileSizeSwitch))) {
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(kBlobFileTransportMinFileSizeSwitch),
        &min_page_file_size))
        << "Unable to parse "
        << command_line->GetSwitchValueASCII(
               kBlobSharedMemoryTransportMaxSizeSwitch);
  }
  if (UNLIKELY(command_line->HasSwitch(kBlobFileTransportMaxFileSizeSwitch))) {
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(kBlobFileTransportMaxFileSizeSwitch),
        &max_file_size))
        << "Unable to parse "
        << command_line->GetSwitchValueASCII(
               kBlobSharedMemoryTransportMaxSizeSwitch);
  }
  CHECK(IsValid());
}
BlobStorageLimits::~BlobStorageLimits() {}

BlobStorageLimits::BlobStorageLimits(const BlobStorageLimits&) = default;
BlobStorageLimits& BlobStorageLimits::operator=(const BlobStorageLimits&) =
    default;

bool BlobStorageLimits::IsValid() const {
  return max_ipc_memory_size <= max_bytes_data_item_size &&
         max_shared_memory_size <= max_bytes_data_item_size &&
         min_page_file_size <= max_file_size &&
         min_page_file_size <= max_blob_in_memory_space &&
         effective_max_disk_space <= desired_max_disk_space;
}

bool BlobStatusIsError(BlobStatus status) {
  return static_cast<int>(status) <= static_cast<int>(BlobStatus::LAST_ERROR);
}

bool BlobStatusIsPending(BlobStatus status) {
  int status_int = static_cast<int>(status);
  return status_int >= static_cast<int>(BlobStatus::PENDING_QUOTA) &&
         status_int <= static_cast<int>(BlobStatus::LAST_PENDING);
}

bool BlobStatusIsBadIPC(BlobStatus status) {
  return status == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
}

}  // namespace storage
