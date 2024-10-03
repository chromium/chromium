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

#include <limits>
#include <optional>
#include <string>

#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "url/gurl.h"

namespace blink {

// static
FileMetadata FileMetadata::From(const base::File::Info& file_info) {
  FileMetadata file_metadata;
  file_metadata.modification_time =
      NullableTimeToOptionalTime(file_info.last_modified);
  file_metadata.length = file_info.size;
  if (file_info.is_directory)
    file_metadata.type = FileMetadata::kTypeDirectory;
  else
    file_metadata.type = FileMetadata::kTypeFile;
  return file_metadata;
}

bool GetFileSize(const String& path,
                 const MojoBindingContext& context,
                 int64_t& result) {
  FileMetadata metadata;
  if (!GetFileMetadata(path, context, metadata))
    return false;
  result = metadata.length;
  return true;
}

bool GetFileMetadata(const String& path,
                     const MojoBindingContext& context,
                     FileMetadata& metadata) {
  mojo::Remote<mojom::blink::FileUtilitiesHost> host;
  context.GetBrowserInterfaceBroker().GetInterface(
      host.BindNewPipeAndPassReceiver());

  std::optional<base::File::Info> file_info;
  if (!host->GetFileInfo(WebStringToFilePath(path), &file_info) || !file_info)
    return false;

  metadata.modification_time =
      NullableTimeToOptionalTime(file_info->last_modified);
  metadata.length = file_info->size;
  metadata.type = file_info->is_directory ? FileMetadata::kTypeDirectory
                                          : FileMetadata::kTypeFile;
  return true;
}

KURL FilePathToURL(const String& path) {
  base::FilePath file_path = WebStringToFilePath(path);
#if BUILDFLAG(IS_ANDROID)
  GURL gurl = file_path.IsContentUri() ? GURL(file_path.value())
                                       : net::FilePathToFileURL(file_path);
#else
  GURL gurl = net::FilePathToFileURL(file_path);
#endif
  const std::string& url_spec = gurl.possibly_invalid_spec();
  return KURL(AtomicString::FromUTF8(url_spec.data(), url_spec.length()),
              gurl.parsed_for_possibly_invalid_spec(), gurl.is_valid());
}

}  // namespace blink
