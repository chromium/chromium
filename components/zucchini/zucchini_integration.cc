// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/zucchini_integration.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/mapped_file.h"
#include "components/zucchini/patch_reader.h"

#if BUILDFLAG(IS_WIN)
#include <ntstatus.h>

#include "components/zucchini/exception_filter_helper_win.h"
#endif

namespace zucchini {

namespace {

struct FileNames {
  FileNames() : is_dummy(true) {
    // Use fake names.
    old_name = old_name.AppendASCII("old_name");
    new_name = new_name.AppendASCII("new_name");
    patch_name = patch_name.AppendASCII("patch_name");
  }

  FileNames(const base::FilePath& old_name,
            const base::FilePath& new_name,
            const base::FilePath& patch_name)
      : old_name(old_name),
        new_name(new_name),
        patch_name(patch_name),
        is_dummy(false) {}

  base::FilePath old_name;
  base::FilePath new_name;
  base::FilePath patch_name;

  // A flag to decide whether the filenames are only for error output.
  const bool is_dummy;
};

status::Code GenerateCommon(base::File old_file,
                            base::File new_file,
                            base::File patch_file,
                            const FileNames& names,
                            bool force_keep,
                            bool is_raw,
                            std::string imposed_matches) {
  MappedFileReader mapped_old(std::move(old_file));
  if (mapped_old.HasError()) {
    LOG(ERROR) << "Error with file " << names.old_name.value() << ": "
               << mapped_old.error();
    return status::kStatusFileReadError;
  }

  MappedFileReader mapped_new(std::move(new_file));
  if (mapped_new.HasError()) {
    LOG(ERROR) << "Error with file " << names.new_name.value() << ": "
               << mapped_new.error();
    return status::kStatusFileReadError;
  }

  status::Code result = status::kStatusSuccess;
  EnsemblePatchWriter patch_writer(mapped_old.region(), mapped_new.region());
  if (is_raw) {
    result = GenerateBufferRaw(mapped_old.region(), mapped_new.region(),
                               &patch_writer);
  } else {
    result = GenerateBufferImposed(mapped_old.region(), mapped_new.region(),
                                   std::move(imposed_matches), &patch_writer);
  }
  if (result != status::kStatusSuccess) {
    LOG(ERROR) << "Fatal error encountered when generating patch.";
    return result;
  }

  // By default, delete patch on destruction, to avoid having lingering files in
  // case of a failure. On Windows deletion can be done by the OS.
  MappedFileWriter mapped_patch(names.patch_name, std::move(patch_file),
                                patch_writer.SerializedSize());
  if (mapped_patch.HasError()) {
    LOG(ERROR) << "Error with file " << names.patch_name.value() << ": "
               << mapped_patch.error();
    return status::kStatusFileWriteError;
  }
  if (force_keep)
    mapped_patch.Keep();

  if (!patch_writer.SerializeInto(mapped_patch.region()))
    return status::kStatusPatchWriteError;

  // Successfully created patch. Explicitly request file to be kept.
  if (!mapped_patch.Keep())
    return status::kStatusFileWriteError;
  return status::kStatusSuccess;
}

status::Code ApplyCommon(base::File old_file,
                         base::File patch_file,
                         base::File new_file,
                         const FileNames& names,
                         bool force_keep) {
#if BUILDFLAG(IS_WIN)
  ExceptionFilterHelper exception_filter_helper;
  __try {
#endif
    MappedFileReader mapped_patch(std::move(patch_file));
    if (mapped_patch.HasError()) {
      LOG(ERROR) << "Error with file " << names.patch_name.value() << ": "
                 << mapped_patch.error();
      return status::kStatusFileReadError;
    }
#if BUILDFLAG(IS_WIN)
    exception_filter_helper.AddRange(
        {mapped_patch.data(), mapped_patch.length()});
#endif

    auto patch_reader = EnsemblePatchReader::Create(mapped_patch.region());
    if (!patch_reader.has_value()) {
      LOG(ERROR) << "Error reading patch header.";
      return status::kStatusPatchReadError;
    }

    MappedFileReader mapped_old(std::move(old_file));
    if (mapped_old.HasError()) {
      LOG(ERROR) << "Error with file " << names.old_name.value() << ": "
                 << mapped_old.error();
      return status::kStatusFileReadError;
    }
#if BUILDFLAG(IS_WIN)
    exception_filter_helper.AddRange({mapped_old.data(), mapped_old.length()});
#endif

    PatchHeader header = patch_reader->header();
    // By default, delete output on destruction, to avoid having lingering files
    // in case of a failure. On Windows deletion can be done by the OS.
    MappedFileWriter mapped_new(names.new_name, std::move(new_file),
                                header.new_size);
    if (mapped_new.HasError()) {
      LOG(ERROR) << "Error with file " << names.new_name.value() << ": "
                 << mapped_new.error();
      return status::kStatusFileWriteError;
    }
    if (force_keep) {
      mapped_new.Keep();
    }
#if BUILDFLAG(IS_WIN)
    exception_filter_helper.AddRange({mapped_new.data(), mapped_new.length()});
#endif

    status::Code result =
        ApplyBuffer(mapped_old.region(), *patch_reader, mapped_new.region());
    if (result != status::kStatusSuccess) {
      LOG(ERROR) << "Fatal error encountered while applying patch.";
      return result;
    }

    // Successfully patch |mapped_new|. Explicitly request file to be kept.
    return mapped_new.Keep() ? status::kStatusSuccess
                             : status::kStatusFileWriteError;
#if BUILDFLAG(IS_WIN)
  } __except (exception_filter_helper.FilterPageError(
      GetExceptionInformation()->ExceptionRecord)) {
    LOG(ERROR) << "EXCEPTION_IN_PAGE_ERROR while "
               << (exception_filter_helper.is_write() ? "writing to"
                                                      : "reading from")
               << " mapped files; NTSTATUS = "
               << exception_filter_helper.nt_status();
    return exception_filter_helper.nt_status() == STATUS_DISK_FULL
               ? status::kStatusDiskFull
               : status::kStatusIoError;
  }
#endif  // BUILDFLAG(IS_WIN)
}

status::Code VerifyPatchCommon(base::File patch_file,
                               base::FilePath patch_name) {
  MappedFileReader mapped_patch(std::move(patch_file));
  if (mapped_patch.HasError()) {
    LOG(ERROR) << "Error with file " << patch_name.value() << ": "
               << mapped_patch.error();
    return status::kStatusFileReadError;
  }
  auto patch_reader = EnsemblePatchReader::Create(mapped_patch.region());
  if (!patch_reader.has_value()) {
    LOG(ERROR) << "Error reading patch header.";
    return status::kStatusPatchReadError;
  }
  return status::kStatusSuccess;
}

}  // namespace

status::Code Generate(base::File old_file,
                      base::File new_file,
                      base::File patch_file,
                      bool force_keep,
                      bool is_raw,
                      std::string imposed_matches) {
  const FileNames file_names;
  return GenerateCommon(std::move(old_file), std::move(new_file),
                        std::move(patch_file), file_names, force_keep, is_raw,
                        std::move(imposed_matches));
}

status::Code Generate(const base::FilePath& old_path,
                      const base::FilePath& new_path,
                      const base::FilePath& patch_path,
                      bool force_keep,
                      bool is_raw,
                      std::string imposed_matches) {
  using base::File;
  File old_file(old_path, File::FLAG_OPEN | File::FLAG_READ |
                              base::File::FLAG_WIN_SHARE_DELETE);
  File new_file(new_path, File::FLAG_OPEN | File::FLAG_READ |
                              base::File::FLAG_WIN_SHARE_DELETE);
  File patch_file(patch_path, File::FLAG_CREATE_ALWAYS | File::FLAG_READ |
                                  File::FLAG_WRITE |
                                  File::FLAG_WIN_SHARE_DELETE |
                                  File::FLAG_CAN_DELETE_ON_CLOSE);
  const FileNames file_names(old_path, new_path, patch_path);
  return GenerateCommon(std::move(old_file), std::move(new_file),
                        std::move(patch_file), file_names, force_keep, is_raw,
                        std::move(imposed_matches));
}

status::Code Apply(base::File old_file,
                   base::File patch_file,
                   base::File new_file,
                   bool force_keep) {
  const FileNames file_names;
  return ApplyCommon(std::move(old_file), std::move(patch_file),
                     std::move(new_file), file_names, force_keep);
}

status::Code Apply(const base::FilePath& old_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& new_path,
                   bool force_keep) {
  using base::File;
  File old_file(old_path, File::FLAG_OPEN | File::FLAG_READ |
                              base::File::FLAG_WIN_SHARE_DELETE);
  File patch_file(patch_path, File::FLAG_OPEN | File::FLAG_READ |
                                  base::File::FLAG_WIN_SHARE_DELETE);
  File new_file(new_path, File::FLAG_CREATE_ALWAYS | File::FLAG_READ |
                              File::FLAG_WRITE | File::FLAG_WIN_SHARE_DELETE |
                              File::FLAG_CAN_DELETE_ON_CLOSE);
  const FileNames file_names(old_path, new_path, patch_path);
  return ApplyCommon(std::move(old_file), std::move(patch_file),
                     std::move(new_file), file_names, force_keep);
}

status::Code VerifyPatch(base::File patch_file) {
  return VerifyPatchCommon(std::move(patch_file), base::FilePath());
}

status::Code VerifyPatch(const base::FilePath& patch_path) {
  using base::File;
  File patch_file(patch_path, File::FLAG_OPEN | File::FLAG_READ |
                                  base::File::FLAG_WIN_SHARE_DELETE);
  return VerifyPatchCommon(std::move(patch_file), patch_path);
}

}  // namespace zucchini
