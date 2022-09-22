/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/file.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_property_bag.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static String GetContentTypeFromFileName(const String& name,
                                         File::ContentTypeLookupPolicy policy) {
  String type;
  wtf_size_t index = name.ReverseFind('.');
  if (index != WTF::kNotFound) {
    if (policy == File::kWellKnownContentTypes) {
      type = MIMETypeRegistry::GetWellKnownMIMETypeForExtension(
          name.Substring(index + 1));
    } else {
      DCHECK_EQ(policy, File::kAllContentTypes);
      type =
          MIMETypeRegistry::GetMIMETypeForExtension(name.Substring(index + 1));
    }
  }
  return type;
}

static std::unique_ptr<BlobData> CreateBlobDataForFileWithType(
    const String& path,
    const String& content_type) {
  std::unique_ptr<BlobData> blob_data =
      BlobData::CreateForFileWithUnknownSize(path);
  blob_data->SetContentType(content_type);
  return blob_data;
}

static std::unique_ptr<BlobData> CreateBlobDataForFile(
    const String& path,
    File::ContentTypeLookupPolicy policy) {
  if (path.empty()) {
    auto blob_data = std::make_unique<BlobData>();
    blob_data->SetContentType("application/octet-stream");
    return blob_data;
  }
  return CreateBlobDataForFileWithType(
      path, GetContentTypeFromFileName(path, policy));
}

static std::unique_ptr<BlobData> CreateBlobDataForFileWithName(
    const String& path,
    const String& file_system_name,
    File::ContentTypeLookupPolicy policy) {
  return CreateBlobDataForFileWithType(
      path, GetContentTypeFromFileName(file_system_name, policy));
}

static std::unique_ptr<BlobData> CreateBlobDataForFileWithMetadata(
    const String& file_system_name,
    const FileMetadata& metadata) {
  std::unique_ptr<BlobData> blob_data;
  if (metadata.length == BlobData::kToEndOfFile) {
    blob_data = BlobData::CreateForFileWithUnknownSize(
        metadata.platform_path, metadata.modification_time);
  } else {
    blob_data = std::make_unique<BlobData>();
    blob_data->AppendFile(metadata.platform_path, 0, metadata.length,
                          metadata.modification_time);
  }
  blob_data->SetContentType(GetContentTypeFromFileName(
      file_system_name, File::kWellKnownContentTypes));
  return blob_data;
}

// static
File* File::Create(ExecutionContext* context,
                   const HeapVector<Member<V8BlobPart>>& file_bits,
                   const String& file_name,
                   const FilePropertyBag* options) {
  DCHECK(options->hasType());

  base::Time last_modified;
  if (options->hasLastModified()) {
    // We don't use base::Time::FromJsTime(double) here because
    // options->lastModified() is a 64-bit integer, and casting it to
    // double is lossy.
    last_modified =
        base::Time::UnixEpoch() + base::Milliseconds(options->lastModified());
  } else {
    last_modified = base::Time::Now();
  }
  DCHECK(options->hasEndings());
  bool normalize_line_endings_to_native = options->endings() == "native";
  if (normalize_line_endings_to_native)
    UseCounter::Count(context, WebFeature::kFileAPINativeLineEndings);

  auto blob_data = std::make_unique<BlobData>();
  blob_data->SetContentType(NormalizeType(options->type()));
  PopulateBlobData(blob_data.get(), file_bits,
                   normalize_line_endings_to_native);

  uint64_t file_size = blob_data->length();
  return MakeGarbageCollected<File>(
      file_name, last_modified,
      BlobDataHandle::Create(std::move(blob_data), file_size));
}

File* File::CreateFromControlState(const FormControlState& state,
                                   wtf_size_t& index) {
  if (index + 2 >= state.ValueSize()) {
    index = state.ValueSize();
    return nullptr;
  }
  String path = state[index++];
  String name = state[index++];
  String relative_path = state[index++];
  if (relative_path.empty())
    return File::CreateForUserProvidedFile(path, name);
  return File::CreateWithRelativePath(path, relative_path);
}

String File::PathFromControlState(const FormControlState& state,
                                  wtf_size_t& index) {
  if (index + 2 >= state.ValueSize()) {
    index = state.ValueSize();
    return String();
  }
  String path = state[index];
  index += 3;
  return path;
}

File* File::CreateWithRelativePath(const String& path,
                                   const String& relative_path) {
  File* file = MakeGarbageCollected<File>(path, File::kAllContentTypes,
                                          File::kIsUserVisible);
  file->relative_path_ = relative_path;
  return file;
}

// static
File* File::CreateForFileSystemFile(ExecutionContext& context,
                                    const KURL& url,
                                    const FileMetadata& metadata,
                                    UserVisibility user_visibility) {
  String content_type =
      GetContentTypeFromFileName(url.GetPath(), File::kWellKnownContentTypes);
  // RegisterBlob doesn't take nullable strings.
  if (content_type.IsNull()) {
    content_type = g_empty_string;
  }

  scoped_refptr<BlobDataHandle> handle;
  CoreInitializer::GetInstance().GetFileSystemManager(&context).RegisterBlob(
      content_type, url, metadata.length, metadata.modification_time, &handle);

  return MakeGarbageCollected<File>(url, metadata, user_visibility, handle);
}

File::File(const String& path,
           ContentTypeLookupPolicy policy,
           UserVisibility user_visibility)
    : Blob(BlobDataHandle::Create(CreateBlobDataForFile(path, policy),
                                  std::numeric_limits<uint64_t>::max())),
      has_backing_file_(true),
      user_visibility_(user_visibility),
      path_(path),
      name_(FilePathToWebString(WebStringToFilePath(path).BaseName())) {}

File::File(const String& path,
           const String& name,
           ContentTypeLookupPolicy policy,
           UserVisibility user_visibility)
    : Blob(BlobDataHandle::Create(
          CreateBlobDataForFileWithName(path, name, policy),
          std::numeric_limits<uint64_t>::max())),
      has_backing_file_(true),
      user_visibility_(user_visibility),
      path_(path),
      name_(name) {}

File::File(const String& path,
           const String& name,
           const String& relative_path,
           UserVisibility user_visibility,
           bool has_snapshot_data,
           uint64_t size,
           const absl::optional<base::Time>& last_modified,
           scoped_refptr<BlobDataHandle> blob_data_handle)
    : Blob(std::move(blob_data_handle)),
      has_backing_file_(!path.empty() || !relative_path.empty()),
      user_visibility_(user_visibility),
      path_(path),
      name_(name),
      snapshot_modification_time_(last_modified),
      relative_path_(relative_path) {
  if (has_snapshot_data)
    snapshot_size_ = size;
}

File::File(const String& name,
           const absl::optional<base::Time>& modification_time,
           scoped_refptr<BlobDataHandle> blob_data_handle)
    : Blob(std::move(blob_data_handle)),
      has_backing_file_(false),
      user_visibility_(File::kIsNotUserVisible),
      name_(name),
      snapshot_modification_time_(modification_time) {
  uint64_t size = Blob::size();
  if (size != std::numeric_limits<uint64_t>::max())
    snapshot_size_ = size;
}

File::File(const String& name,
           const FileMetadata& metadata,
           UserVisibility user_visibility)
    : Blob(BlobDataHandle::Create(
          CreateBlobDataForFileWithMetadata(name, metadata),
          metadata.length)),
      has_backing_file_(true),
      user_visibility_(user_visibility),
      path_(metadata.platform_path),
      name_(name),
      snapshot_modification_time_(metadata.modification_time) {
  if (metadata.length >= 0)
    snapshot_size_ = metadata.length;
}

File::File(const KURL& file_system_url,
           const FileMetadata& metadata,
           UserVisibility user_visibility,
           scoped_refptr<BlobDataHandle> blob_data_handle)
    : Blob(std::move(blob_data_handle)),
      has_backing_file_(false),
      user_visibility_(user_visibility),
      name_(DecodeURLEscapeSequences(file_system_url.LastPathComponent(),
                                     DecodeURLMode::kUTF8OrIsomorphic)),
      file_system_url_(file_system_url),
      snapshot_size_(metadata.length),
      snapshot_modification_time_(metadata.modification_time) {
  DCHECK_GE(metadata.length, 0);
}

File::File(const File& other)
    : Blob(other.GetBlobDataHandle()),
      has_backing_file_(other.has_backing_file_),
      user_visibility_(other.user_visibility_),
      path_(other.path_),
      name_(other.name_),
      file_system_url_(other.file_system_url_),
      snapshot_size_(other.snapshot_size_),
      snapshot_modification_time_(other.snapshot_modification_time_),
      relative_path_(other.relative_path_) {}

File* File::Clone(const String& name) const {
  File* file = MakeGarbageCollected<File>(*this);
  if (!name.IsNull())
    file->name_ = name;
  return file;
}

base::Time File::LastModifiedTime() const {
  CaptureSnapshotIfNeeded();

  if (HasValidSnapshotMetadata() && snapshot_modification_time_)
    return *snapshot_modification_time_;

  // lastModified / lastModifiedDate getters should return the current time
  // when the last modification time isn't known.
  return base::Time::Now();
}

int64_t File::lastModified() const {
  // lastModified returns a number, not a Date instance,
  // http://dev.w3.org/2006/webapi/FileAPI/#file-attrs
  return (LastModifiedTime() - base::Time::UnixEpoch()).InMilliseconds();
}

ScriptValue File::lastModifiedDate(ScriptState* script_state) const {
  // lastModifiedDate returns a Date instance,
  // http://www.w3.org/TR/FileAPI/#dfn-lastModifiedDate
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLNullable<IDLDate>>::ToV8(
          script_state, absl::optional<base::Time>(LastModifiedTime()))
          .ToLocalChecked());
}

absl::optional<base::Time> File::LastModifiedTimeForSerialization() const {
  CaptureSnapshotIfNeeded();

  return snapshot_modification_time_;
}

uint64_t File::size() const {
  CaptureSnapshotIfNeeded();

  // FIXME: JavaScript cannot represent sizes as large as uint64_t, we need
  // to come up with an exception to throw if file size is not representable.
  if (HasValidSnapshotMetadata())
    return *snapshot_size_;

  return 0;
}

Blob* File::slice(int64_t start,
                  int64_t end,
                  const String& content_type,
                  ExceptionState& exception_state) const {
  if (!has_backing_file_)
    return Blob::slice(start, end, content_type, exception_state);

  // FIXME: Calling size triggers capturing a snapshot, if we don't have one
  // already. This involves synchronous file operation. We need to figure out
  // how to make it asynchronous (or make sure snapshot state is always passed
  // in when creating a File instance).
  ClampSliceOffsets(size(), start, end);

  uint64_t length = end - start;
  auto blob_data = std::make_unique<BlobData>();
  blob_data->SetContentType(NormalizeType(content_type));
  DCHECK(!path_.empty());
  blob_data->AppendFile(path_, start, length, snapshot_modification_time_);
  return MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), length));
}

void File::CaptureSnapshotIfNeeded() const {
  if (HasValidSnapshotMetadata() && snapshot_modification_time_)
    return;

  uint64_t snapshot_size;
  if (GetBlobDataHandle()->CaptureSnapshot(&snapshot_size,
                                           &snapshot_modification_time_)) {
    snapshot_size_ = snapshot_size;
  }
}

void File::AppendTo(BlobData& blob_data) const {
  if (!has_backing_file_) {
    Blob::AppendTo(blob_data);
    return;
  }

  // FIXME: This involves synchronous file operation. We need to figure out how
  // to make it asynchronous.
  CaptureSnapshotIfNeeded();
  DCHECK(!path_.empty());
  blob_data.AppendFile(path_, 0, *snapshot_size_, snapshot_modification_time_);
}

bool File::HasSameSource(const File& other) const {
  if (has_backing_file_ != other.has_backing_file_)
    return false;

  if (has_backing_file_)
    return path_ == other.path_;

  if (file_system_url_.IsEmpty() != other.file_system_url_.IsEmpty())
    return false;

  if (!file_system_url_.IsEmpty())
    return file_system_url_ == other.file_system_url_;

  return Uuid() == other.Uuid();
}

bool File::AppendToControlState(FormControlState& state) {
  // FIXME: handle Blob-backed File instances, see http://crbug.com/394948
  if (!HasBackingFile())
    return false;
  state.Append(GetPath());
  state.Append(name());
  state.Append(webkitRelativePath());
  return true;
}

}  // namespace blink
