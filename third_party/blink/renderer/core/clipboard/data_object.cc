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

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/dragged_isolated_file_system.h"
#include "third_party/blink/renderer/core/clipboard/paste_mode.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

DataObject* DataObject::CreateFromClipboard(PasteMode paste_mode) {
  DataObject* data_object = Create();
#if DCHECK_IS_ON()
  HashSet<String> types_seen;
#endif
  uint64_t sequence_number = SystemClipboard::GetInstance().SequenceNumber();
  for (const String& type :
       SystemClipboard::GetInstance().ReadAvailableTypes()) {
    if (paste_mode == PasteMode::kPlainTextOnly && type != kMimeTypeTextPlain)
      continue;
    data_object->item_list_.push_back(
        DataObjectItem::CreateFromClipboard(type, sequence_number));
#if DCHECK_IS_ON()
    DCHECK(types_seen.insert(type).is_new_entry);
#endif
  }
  return data_object;
}

DataObject* DataObject::CreateFromString(const String& data) {
  DataObject* data_object = Create();
  data_object->Add(data, kMimeTypeTextPlain);
  return data_object;
}

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
  return item_list_[index];
}

void DataObject::DeleteItem(uint32_t index) {
  if (index >= length())
    return;
  item_list_.EraseAt(index);
  NotifyItemListChanged();
}

void DataObject::ClearAll() {
  if (item_list_.IsEmpty())
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
    NOTREACHED();
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

void DataObject::AddFilename(const String& filename,
                             const String& display_name,
                             const String& file_system_id) {
  InternalAddFileItem(DataObjectItem::CreateFromFileWithFileSystemId(
      File::CreateForUserProvidedFile(filename, display_name), file_system_id));
}

void DataObject::AddSharedBuffer(scoped_refptr<SharedBuffer> buffer,
                                 const KURL& source_url,
                                 const String& filename_extension,
                                 const AtomicString& content_disposition) {
  InternalAddFileItem(DataObjectItem::CreateFromSharedBuffer(
      std::move(buffer), source_url, filename_extension, content_disposition));
}

DataObject::DataObject() : modifiers_(0) {}

DataObjectItem* DataObject::FindStringItem(const String& type) const {
  for (const auto& item : item_list_) {
    if (item->Kind() == DataObjectItem::kStringKind && item->GetType() == type)
      return item;
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

void DataObject::Trace(blink::Visitor* visitor) {
  visitor->Trace(item_list_);
  visitor->Trace(observers_);
  Supplementable<DataObject>::Trace(visitor);
}

DataObject* DataObject::Create(WebDragData data) {
  DataObject* data_object = Create();
  bool has_file_system = false;

  for (const WebDragData::Item& item : data.Items()) {
    switch (item.storage_type) {
      case WebDragData::Item::kStorageTypeString:
        if (String(item.string_type) == kMimeTypeTextURIList)
          data_object->SetURLAndTitle(item.string_data, item.title);
        else if (String(item.string_type) == kMimeTypeTextHTML)
          data_object->SetHTMLAndBaseURL(item.string_data, item.base_url);
        else
          data_object->SetData(item.string_type, item.string_data);
        break;
      case WebDragData::Item::kStorageTypeFilename:
        has_file_system = true;
        data_object->AddFilename(item.filename_data, item.display_name_data,
                                 data.FilesystemId());
        break;
      case WebDragData::Item::kStorageTypeBinaryData:
        // This should never happen when dragging in.
        break;
      case WebDragData::Item::kStorageTypeFileSystemFile: {
        // TODO(http://crbug.com/429077): The file system URL may refer a user
        // visible file.
        has_file_system = true;
        FileMetadata file_metadata;
        file_metadata.length = item.file_system_file_size;

        data_object->Add(
            File::CreateForFileSystemFile(item.file_system_url, file_metadata,
                                          File::kIsNotUserVisible),
            item.file_system_id);
      } break;
    }
  }

  data_object->SetFilesystemId(data.FilesystemId());

  if (has_file_system)
    DraggedIsolatedFileSystem::PrepareForDataObject(data_object);

  return data_object;
}

WebDragData DataObject::ToWebDragData() {
  WebDragData data;
  data.Initialize();
  data.SetModifierKeyState(modifiers_);
  WebVector<WebDragData::Item> item_list(length());

  for (wtf_size_t i = 0; i < length(); ++i) {
    DataObjectItem* original_item = Item(i);
    WebDragData::Item item;
    if (original_item->Kind() == DataObjectItem::kStringKind) {
      item.storage_type = WebDragData::Item::kStorageTypeString;
      item.string_type = original_item->GetType();
      item.string_data = original_item->GetAsString();
      item.title = original_item->Title();
      item.base_url = original_item->BaseURL();
    } else if (original_item->Kind() == DataObjectItem::kFileKind) {
      if (original_item->GetSharedBuffer()) {
        item.storage_type = WebDragData::Item::kStorageTypeBinaryData;
        item.binary_data = original_item->GetSharedBuffer();
        item.binary_data_source_url = original_item->BaseURL();
        item.binary_data_filename_extension =
            original_item->FilenameExtension();
        item.binary_data_content_disposition = original_item->Title();
      } else if (original_item->IsFilename()) {
        Blob* blob = original_item->GetAsFile();
        if (auto* file = DynamicTo<File>(blob)) {
          if (file->HasBackingFile()) {
            item.storage_type = WebDragData::Item::kStorageTypeFilename;
            item.filename_data = file->GetPath();
            item.display_name_data = file->name();
          } else if (!file->FileSystemURL().IsEmpty()) {
            item.storage_type = WebDragData::Item::kStorageTypeFileSystemFile;
            item.file_system_url = file->FileSystemURL();
            item.file_system_file_size = file->size();
            item.file_system_id = original_item->FileSystemId();
          } else {
            // TODO(http://crbug.com/394955): support dragging constructed Files
            // across renderers.
            item.storage_type = WebDragData::Item::kStorageTypeString;
            item.string_type = "text/plain";
            item.string_data = file->name();
          }
        } else {
          NOTREACHED();
        }
      } else {
        NOTREACHED();
      }
    } else {
      NOTREACHED();
    }
    item_list[i] = item;
  }
  data.SwapItems(item_list);
  return data;
}

}  // namespace blink
