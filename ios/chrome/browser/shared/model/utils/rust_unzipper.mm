// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/rust_unzipper.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span_rust.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/shared/model/utils/zip_ffi_glue.rs.h"

using rust_zip::RustFileData;
using rust_zip::unzip_archive_in_memory;
using rust_zip::UnzipResult;

namespace {

NSString* const kRustUnzipperErrorDomain = @"rust_unzipper_error_domain";

constexpr char kEmptyDataErrorDescription[] = "Input NSData is empty.";

constexpr NSInteger kDefaultErrorCode = -1;

// Creates a standard error object from the unzip failure details.
NSError* ParseUnzipErrorAsNSError(const std::string& message,
                                  NSInteger error_code = kDefaultErrorCode) {
  NSString* error_message =
      message.empty()
          ? [NSString stringWithFormat:@"Rust unzipping failed with code: %d "
                                       @"(no message provided)",
                                       static_cast<int>(error_code)]
          : base::SysUTF8ToNSString(message);

  NSDictionary* user_info = @{NSLocalizedDescriptionKey : error_message};

  return [NSError errorWithDomain:kRustUnzipperErrorDomain
                             code:error_code
                         userInfo:user_info];
}

// Synchronously unzips the provided data, returning the contained files or an
// error.
UnzipResultData UnzipDataSync(NSData* data) {
  if (data.length == 0) {
    return {.error = ParseUnzipErrorAsNSError(kEmptyDataErrorDescription)};
  }

  std::vector<RustFileData> file_contents;
  std::string error_string;

  if (UnzipResult result_code = unzip_archive_in_memory(
          SpanToRustSlice(base::apple::NSDataToSpan(data)), file_contents,
          error_string);
      result_code != UnzipResult::Success) {
    return {.error = ParseUnzipErrorAsNSError(
                error_string, static_cast<NSInteger>(result_code))};
  }

  NSMutableArray<NSData*>* results_array =
      [NSMutableArray arrayWithCapacity:file_contents.size()];

  for (const auto& single_file_contents : file_contents) {
    NSData* ns_data_entry =
        [NSData dataWithBytes:single_file_contents.data.data()
                       length:single_file_contents.data.size()];
    [results_array addObject:ns_data_entry];
  }

  return {.unzipped_files = results_array};
}

}  // namespace

void UnzipData(NSData* data,
               base::OnceCallback<void(UnzipResultData)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&UnzipDataSync, data),
      std::move(callback));
}
