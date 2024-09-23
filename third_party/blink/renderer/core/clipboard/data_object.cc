/*
 * Copyright (c) 2008, 2009, 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/clipboard/data_object.h"

#include <utility>

#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/dragged_isolated_file_system.h"
#include "third_party/blink/renderer/core/clipboard/paste_mode.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// static
DataObject* DataObject::CreateFromClipboard(ExecutionContext* context,
                                            SystemClipboard* system_clipboard,
                                            PasteMode paste_mode) {
  DataObject* data_object = Create();
#if DCHECK_IS_ON()
  HashSet<String> types_seen;
#endif
  ClipboardSequenceNumberToken sequence_number =
      system_clipboard->SequenceNumber();
  for (const String& type : system_clipboard->ReadAvailableTypes()) {
    if (paste_mode == PasteMode::kPlainTextOnly && type != kMimeTypeTextPlain)
      continue;
    mojom::blink::ClipboardFilesPtr files;
    if (type == kMimeTypeTextURIList) {
      files = system_clipboard->ReadFiles();
      if (files) {
        // Ignore ReadFiles() result if clipboard sequence number has changed.
        if (system_clipboard->SequenceNumber() != sequence_number) {
          files->files.clear();
        } else {
          for (const mojom::blink::DataTransferFilePtr& file : files->files) {
            data_object->AddFilename(
                context, FilePathToString(file->path),
                FilePathToString(file->display_name), files->file_system_id,
                base::MakeRefCounted<FileSystemAccessDropData>(
                    std::move(file->file_system_access_token)));
          }
        }
      }
    }
    if (files && !files->files.empty()) {
      DraggedIsolatedFileSystem::PrepareForDataObject(data_object);
    } else {
      data_object->item_list_.push_back(DataObjectItem::CreateFromClipboard(
          system_clipboard, type, sequence_number));
    }
#if DCHECK_IS_ON()
    DCHECK(types_seen.insert(type).is_new_entry);
#endif
  }
  return data_object;
}

DataObject* DataObject::CreateFromClipboard(SystemClipboard* system_clipboard,
                                            PasteMode paste_mode) {
  return CreateFromClipboard(/*context=*/nullptr, system_clipboard, paste_mode);
}

// static
DataObject* DataObject::CreateFromString(const String& data) {
  DataObject* data_object = Create();
  data_object->Add(data, kMimeTypeTextPlain);
  return data_object;
}

// static
DataObject* DataObject::Create() {
  return MakeGarbageCollected<DataObject>();
}

DataObject::~DataObject() = default;

uint32_t DataObject::length() const {
  return item_list_.size();
}

DataObjectItem* DataObject::Item(uint32_t index) {
  if (index >= length())
    return nullptr;
  return item_list_[index].Get();
}

void DataObject::DeleteItem(uint32_t index) {
  if (index >= length())
    return;
  item_list_.EraseAt(index);
  NotifyItemListChanged();
}

void DataObject::ClearStringItems() {
  if (item_list_.empty()) {
    return;
  }

  wtf_size_t num_items_before = item_list_.size();
  item_list_.erase(std::remove_if(item_list_.begin(), item_list_.end(),
                                  [](Member<DataObjectItem> item) {
                                    return item->Kind() ==
                                           DataObjectItem::kStringKind;
                                  }),
                   item_list_.end());
  if (num_items_before != item_list_.size()) {
    NotifyItemListChanged();
  }
}

void DataObject::ClearAll() {
  if (item_list_.empty())
    return;
  item_list_.clear();
  NotifyItemListChanged();
}

DataObjectItem* DataObject::Add(const String& data, const String& type) {
  DataObjectItem* item = DataObjectItem::CreateFromString(type, data);
  if (!InternalAddStringItem(item))
    return nullptr;
  return item;
}

DataObjectItem* DataObject::Add(File* file) {
  if (!file)
    return nullptr;

  DataObjectItem* item = DataObjectItem::CreateFromFile(file);
  InternalAddFileItem(item);
  return item;
}

DataObjectItem* DataObject::Add(File* file, const String& file_system_id) {
  if (!file)
    return nullptr;

  DataObjectItem* item =
      DataObjectItem::CreateFromFileWithFileSystemId(file, file_system_id);
  InternalAddFileItem(item);
  return item;
}

void DataObject::ClearData(const String& type) {
  for (wtf_size_t i = 0; i < item_list_.size(); ++i) {
    if (item_list_[i]->Kind() == DataObjectItem::kStringKind &&
        item_list_[i]->GetType() == type) {
      // Per the spec, type must be unique among all items of kind 'string'.
      item_list_.EraseAt(i);
      NotifyItemListChanged();
      return;
    }
  }
}

Vector<String> DataObject::Types() const {
  Vector<String> results;
#if DCHECK_IS_ON()
  HashSet<String> types_seen;
#endif
  bool contains_files = false;
  for (const auto& item : item_list_) {
    switch (item->Kind()) {
      case DataObjectItem::kStringKind:
        // Per the spec, type must be unique among all items of kind 'string'.
        results.push_back(item->GetType());
#if DCHECK_IS_ON()
        DCHECK(types_seen.insert(item->GetType()).is_new_entry);
#endif
        break;
      case DataObjectItem::kFileKind:
        contains_files = true;
        break;
    }
  }
  if (contains_files) {
    results.push_back(kMimeTypeFiles);
#if DCHECK_IS_ON()
    DCHECK(types_seen.insert(kMimeTypeFiles).is_new_entry);
#endif
  }
  return results;
}

String DataObject::GetData(const String& type) const {
  for (const auto& item : item_list_) {
    if (item->Kind() == DataObjectItem::kStringKind && item->GetType() == type)
      return item->GetAsString();
  }
  return String();
}

void DataObject::SetData(const String& type, const String& data) {
  ClearData(type);
  if (!Add(data, type))
    NOTREACHED_IN_MIGRATION();
}

void DataObject::UrlAndTitle(String& url, String* title) const {
  DataObjectItem* item = FindStringItem(kMimeTypeTextURIList);
  if (!item)
    return;
  url = ConvertURIListToURL(item->GetAsString());
  if (title)
    *title = item->Title();
}

void DataObject::SetURLAndTitle(const String& url, const String& title) {
  ClearData(kMimeTypeTextURIList);
  InternalAddStringItem(DataObjectItem::CreateFromURL(url, title));
}

void DataObject::HtmlAndBaseURL(String& html, KURL& base_url) const {
  DataObjectItem* item = FindStringItem(kMimeTypeTextHTML);
  if (!item)
    return;
  html = item->GetAsString();
  base_url = item->BaseURL();
}

void DataObject::SetHTMLAndBaseURL(const String& html, const KURL& base_url) {
  ClearData(kMimeTypeTextHTML);
  InternalAddStringItem(DataObjectItem::CreateFromHTML(html, base_url));
}

bool DataObject::ContainsFilenames() const {
  for (const auto& item : item_list_) {
    if (item->IsFilename())
      return true;
  }
  return false;
}

Vector<String> DataObject::Filenames() const {
  Vector<String> results;
  for (const auto& item : item_list_) {
    if (item->IsFilename())
      results.push_back(item->GetAsFile()->GetPath());
  }
  return results;
}

void DataObject::AddFilename(
    ExecutionContext* context,
    const String& filename,
    const String& display_name,
    const String& file_system_id,
    scoped_refptr<FileSystemAccessDropData> file_system_access_entry) {
  InternalAddFileItem(DataObjectItem::CreateFromFileWithFileSystemId(
      File::CreateForUserProvidedFile(context, filename, display_name),
      file_system_id, std::move(file_system_access_entry)));
}

void DataObject::AddFileSharedBuffer(scoped_refptr<SharedBuffer> buffer,
                                     bool is_image_accessible,
                                     const KURL& source_url,
                                     const String& filename_extension,
                                     const AtomicString& content_disposition) {
  InternalAddFileItem(DataObjectItem::CreateFromFileSharedBuffer(
      std::move(buffer), is_image_accessible, source_url, filename_extension,
      content_disposition));
}

DataObject::DataObject() : modifiers_(0) {}

DataObjectItem* DataObject::FindStringItem(const String& type) const {
  for (const auto& item : item_list_) {
    if (item->Kind() == DataObjectItem::kStringKind && item->GetType() == type)
      return item.Get();
  }
  return nullptr;
}

bool DataObject::InternalAddStringItem(DataObjectItem* new_item) {
  DCHECK_EQ(new_item->Kind(), DataObjectItem::kStringKind);
  for (const auto& item : item_list_) {
    if (item->Kind() == DataObjectItem::kStringKind &&
        item->GetType() == new_item->GetType())
      return false;
  }

  item_list_.push_back(new_item);
  NotifyItemListChanged();
  return true;
}

void DataObject::InternalAddFileItem(DataObjectItem* new_item) {
  DCHECK_EQ(new_item->Kind(), DataObjectItem::kFileKind);
  item_list_.push_back(new_item);
  NotifyItemListChanged();
}

void DataObject::AddObserver(Observer* observer) {
  DCHECK(!observers_.Contains(observer));
  observers_.insert(observer);
}

void DataObject::NotifyItemListChanged() const {
  for (const Member<Observer>& observer : observers_)
    observer->OnItemListChanged();
}

void DataObject::Trace(Visitor* visitor) const {
  visitor->Trace(item_list_);
  visitor->Trace(observers_);
  Supplementable<DataObject>::Trace(visitor);
}

// static
DataObject* DataObject::Create(ExecutionContext* context,
                               const WebDragData& data) {
  DataObject* data_object = Create();
  bool has_file_system = false;

  for (const WebDragData::Item& item : data.Items()) {
    absl::visit(
        base::Overloaded{
            [&](const WebDragData::StringItem& item) {
              if (String(item.type) == kMimeTypeTextURIList) {
                data_object->SetURLAndTitle(item.data, item.title);
              } else if (String(item.type) == kMimeTypeTextHTML) {
                data_object->SetHTMLAndBaseURL(item.data, item.base_url);
              } else {
                data_object->SetData(item.type, item.data);
              }
            },
            [&](const WebDragData::FilenameItem& item) {
              has_file_system = true;
              data_object->AddFilename(context, item.filename,
                                       item.display_name, data.FilesystemId(),
                                       item.file_system_access_entry);
            },
            [&](const WebDragData::BinaryDataItem& item) {
              data_object->AddFileSharedBuffer(
                  item.data, item.image_accessible, item.source_url,
                  item.filename_extension, item.content_disposition);
            },
            [&](const WebDragData::FileSystemFileItem& item) {
              // TODO(http://crbug.com/429077): The file system URL may refer a
              // user visible file.
              scoped_refptr<BlobDataHandle> blob_data_handle =
                  item.blob_info.GetBlobHandle();

              // If the browser process has provided a BlobDataHandle to use for
              // building the File object (as a result of a drop operation being
              // performed) then use it to create the file here (instead of
              // creating a File object without one and requiring a call to
              // BlobRegistry::Register in the browser process to hook up the
              // Blob remote/receiver pair). If no BlobDataHandle was provided,
              // create a BlobDataHandle to an empty blob since the File object
              // contents won't be needed (for example, because this DataObject
              // will be used for the DragEnter case where the spec only
              // indicates that basic file metadata should be retrievable via
              // the corresponding DataTransferItem).
              if (!blob_data_handle) {
                blob_data_handle = BlobDataHandle::Create();
              }
              has_file_system = true;
              FileMetadata file_metadata;
              file_metadata.length = item.size;
              data_object->Add(
                  File::CreateForFileSystemFile(item.url, file_metadata,
                                                File::kIsNotUserVisible,
                                                std::move(blob_data_handle)),
                  item.file_system_id);
            },
        },
        item);
  }

  data_object->SetFilesystemId(data.FilesystemId());

  if (has_file_system)
    DraggedIsolatedFileSystem::PrepareForDataObject(data_object);

  return data_object;
}

DataObject* DataObject::Create(const WebDragData& data) {
  return Create(/*context=*/nullptr, data);
}

WebDragData DataObject::ToWebDragData() {
  WebDragData data;
  WebVector<WebDragData::Item> item_list(length());

  for (wtf_size_t i = 0; i < length(); ++i) {
    DataObjectItem* original_item = Item(i);
    WebDragData::Item& item = item_list[i];
    switch (original_item->Kind()) {
      case DataObjectItem::kStringKind: {
        auto& string_item = item.emplace<WebDragData::StringItem>();
        string_item.type = original_item->GetType();
        string_item.data = original_item->GetAsString();
        string_item.title = original_item->Title();
        string_item.base_url = original_item->BaseURL();
        break;
      }
      case DataObjectItem::kFileKind: {
        if (original_item->GetSharedBuffer()) {
          auto& binary_data_item = item.emplace<WebDragData::BinaryDataItem>();
          binary_data_item.data = original_item->GetSharedBuffer();
          binary_data_item.image_accessible =
              original_item->IsImageAccessible();
          binary_data_item.source_url = original_item->BaseURL();
          binary_data_item.filename_extension =
              original_item->FilenameExtension();
          binary_data_item.content_disposition = original_item->Title();
        } else if (original_item->IsFilename()) {
          auto* file = original_item->GetAsFile();
          if (file->HasBackingFile()) {
            auto& filename_item =
                item_list[i].emplace<WebDragData::FilenameItem>();
            filename_item.filename = file->GetPath();
            filename_item.display_name = file->name();
          } else if (!file->FileSystemURL().IsEmpty()) {
            auto& file_system_file_item =
                item_list[i].emplace<WebDragData::FileSystemFileItem>();
            file_system_file_item.url = file->FileSystemURL();
            file_system_file_item.size = file->size();
            file_system_file_item.file_system_id =
                original_item->FileSystemId();
          } else {
            // TODO(http://crbug.com/394955): support dragging constructed
            // Files across renderers.
            auto& string_item = item_list[i].emplace<WebDragData::StringItem>();
            string_item.type = "text/plain";
            string_item.data = file->name();
          }
        } else {
          NOTREACHED_IN_MIGRATION();
        }
        break;
      }
    }
  }
  data.SetItems(std::move(item_list));
  return data;
}

}  // namespace blink
