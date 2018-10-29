/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/file_metadata.h"

#include "base/optional.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "url/gurl.h"

namespace blink {

// static
FileMetadata FileMetadata::From(const base::File::Info& file_info) {
  FileMetadata file_metadata;
  if (file_info.last_modified.is_null())
    file_metadata.modification_time = std::numeric_limits<double>::quiet_NaN();
  else
    file_metadata.modification_time = file_info.last_modified.ToJsTime();
  file_metadata.length = file_info.size;
  if (file_info.is_directory)
    file_metadata.type = FileMetadata::kTypeDirectory;
  else
    file_metadata.type = FileMetadata::kTypeFile;
  return file_metadata;
}

bool GetFileSize(const String& path, long long& result) {
  FileMetadata metadata;
  if (!GetFileMetadata(path, metadata))
    return false;
  result = metadata.length;
  return true;
}

bool GetFileModificationTime(const String& path, double& result) {
  FileMetadata metadata;
  if (!GetFileMetadata(path, metadata))
    return false;
  result = metadata.modification_time;
  return true;
}

bool GetFileMetadata(const String& path, FileMetadata& metadata) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojom::blink::FileUtilitiesHostPtr>, thread_specific_host,
      ());
  auto& host = *thread_specific_host;
  if (!host) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&host));
  }

  base::Optional<base::File::Info> file_info;
  if (!host->GetFileInfo(WebStringToFilePath(path), &file_info) || !file_info)
    return false;

  // Blink now expects NaN as uninitialized/null Date.
  metadata.modification_time = file_info->last_modified.is_null()
                                   ? std::numeric_limits<double>::quiet_NaN()
                                   : file_info->last_modified.ToJsTime();
  metadata.length = file_info->size;
  metadata.type = file_info->is_directory ? FileMetadata::kTypeDirectory
                                          : FileMetadata::kTypeFile;
  return true;
}

KURL FilePathToURL(const String& path) {
  GURL gurl = net::FilePathToFileURL(WebStringToFilePath(path));
  const std::string& url_spec = gurl.possibly_invalid_spec();
  return KURL(AtomicString::FromUTF8(url_spec.data(), url_spec.length()),
              gurl.parsed_for_possibly_invalid_spec(), gurl.is_valid());
}

}  // namespace blink
