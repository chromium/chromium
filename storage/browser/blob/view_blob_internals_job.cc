// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/view_blob_internals_job.h"

#include <stddef.h>
#include <stdint.h>
#include <sstream>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_storage_registry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"

namespace {
using storage::BlobStatus;

const char kEmptyBlobStorageMessage[] = "No available blob data.";
const char kContentType[] = "Content Type: ";
const char kContentDisposition[] = "Content Disposition: ";
const char kCount[] = "Count: ";
const char kIndex[] = "Index: ";
const char kType[] = "Type: ";
const char kPath[] = "Path: ";
const char kURL[] = "URL: ";
const char kModificationTime[] = "Modification Time: ";
const char kOffset[] = "Offset: ";
const char kLength[] = "Length: ";
const char kRefcount[] = "Refcount: ";
const char kStatus[] = "Status: ";

void StartHTML(std::string* out) {
  out->append(
      "<!DOCTYPE HTML>"
      "<html><title>Blob Storage Internals</title>"
      "<meta http-equiv=\"Content-Security-Policy\""
      "  content=\"object-src 'none'; script-src 'none'\">\n"
      "<style>\n"
      "body { font-family: sans-serif; font-size: 0.8em; }\n"
      "tt, code, pre { font-family: WebKitHack, monospace; }\n"
      "form { display: inline }\n"
      ".subsection_body { margin: 10px 0 10px 2em; }\n"
      ".subsection_title { font-weight: bold; }\n"
      "</style>\n"
      "</head><body>\n\n");
}

std::string StatusToString(BlobStatus status) {
  switch (status) {
    case BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS:
      return "BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS: Illegal blob "
             "construction.";
    case BlobStatus::ERR_OUT_OF_MEMORY:
      return "BlobStatus::ERR_OUT_OF_MEMORY: Not enough memory or disk space "
             "available for blob.";
    case BlobStatus::ERR_FILE_WRITE_FAILED:
      return "BlobStatus::ERR_FILE_WRITE_FAILED: File operation failed";
    case BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT:
      return "BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT: Blob source died before "
             "transporting data to browser.";
    case BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING:
      return "BlobStatus::ERR_BLOB_DEREFERENCED_WHILE_BUILDING: Blob "
             "references removed while building.";
    case BlobStatus::ERR_REFERENCED_BLOB_BROKEN:
      return "BlobStatus::ERR_REFERENCED_BLOB_BROKEN: Blob contains dependency "
             "blob that is broken.";
    case BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE:
      return "BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE: Blob contains "
             "dependency on file that is unavailable.";
    case BlobStatus::DONE:
      return "BlobStatus::DONE: Blob built with no errors.";
    case BlobStatus::PENDING_QUOTA:
      return "BlobStatus::PENDING_QUOTA: Blob construction is pending on "
             "memory or file quota.";
    case BlobStatus::PENDING_TRANSPORT:
      return "BlobStatus::PENDING_TRANSPORT: Blob construction is pending on "
             "data transport from renderer.";
    case BlobStatus::PENDING_REFERENCED_BLOBS:
      return "BlobStatus::PENDING_REFERENCED_BLOBS: Blob construction is "
             "pending on dependency blobs to finish construction.";
    case BlobStatus::PENDING_CONSTRUCTION:
      return "BlobStatus::PENDING_CONSTRUCTION: Blob construction is pending "
             "on resolving the UUIDs of refereneced blobs.";
  }
  NOTREACHED();
  return "Invalid blob state.";
}

void EndHTML(std::string* out) {
  out->append("\n</body></html>");
}

void AddHTMLBoldText(const std::string& text, std::string* out) {
  out->append("<b>");
  out->append(net::EscapeForHTML(text));
  out->append("</b>");
}

void StartHTMLList(std::string* out) {
  out->append("\n<ul>");
}

void EndHTMLList(std::string* out) {
  out->append("</ul>\n");
}

void AddHTMLListItem(const std::string& element_title,
                     const std::string& element_data,
                     std::string* out) {
  out->append("<li>");
  // No need to escape element_title since constant string is passed.
  out->append(element_title);
  out->append(net::EscapeForHTML(element_data));
  out->append("</li>\n");
}

void AddHorizontalRule(std::string* out) {
  out->append("\n<hr>\n");
}

}  // namespace

namespace storage {

std::string ViewBlobInternalsJob::GenerateHTML(
    BlobStorageContext* blob_storage_context) {
  std::string out;
  StartHTML(&out);
  if (blob_storage_context->registry().blob_map_.empty()) {
    out.append(kEmptyBlobStorageMessage);
  } else {
    for (const auto& uuid_entry_pair :
         blob_storage_context->registry().blob_map_) {
      AddHTMLBoldText(uuid_entry_pair.first, &out);
      BlobEntry* entry = uuid_entry_pair.second.get();
      GenerateHTMLForBlobData(*entry, entry->content_type(),
                              entry->content_disposition(), entry->refcount(),
                              &out);
    }
    if (!blob_storage_context->registry().url_to_blob_.empty()) {
      AddHorizontalRule(&out);
      for (const auto& url_uuid_pair :
           blob_storage_context->registry().url_to_blob_) {
        AddHTMLBoldText(url_uuid_pair.first.spec(), &out);
        // TODO(mek): Somehow include information on the blob the URL is mapped
        // to.
      }
    }
  }
  EndHTML(&out);
  return out;
}

void ViewBlobInternalsJob::GenerateHTMLForBlobData(
    const BlobEntry& blob_data,
    const std::string& content_type,
    const std::string& content_disposition,
    size_t refcount,
    std::string* out) {
  StartHTMLList(out);

  AddHTMLListItem(kRefcount, base::NumberToString(refcount), out);
  AddHTMLListItem(kStatus, StatusToString(blob_data.status()), out);
  if (!content_type.empty())
    AddHTMLListItem(kContentType, content_type, out);
  if (!content_disposition.empty())
    AddHTMLListItem(kContentDisposition, content_disposition, out);

  bool has_multi_items = blob_data.items().size() > 1;
  if (has_multi_items) {
    AddHTMLListItem(kCount,
        base::UTF16ToUTF8(base::FormatNumber(blob_data.items().size())), out);
  }

  for (size_t i = 0; i < blob_data.items().size(); ++i) {
    if (has_multi_items) {
      AddHTMLListItem(kIndex, base::UTF16ToUTF8(base::FormatNumber(i)), out);
      StartHTMLList(out);
    }
    const BlobDataItem& item = *(blob_data.items().at(i)->item());

    switch (item.type()) {
      case BlobDataItem::Type::kBytes:
        AddHTMLListItem(kType, "data", out);
        break;
      case BlobDataItem::Type::kFile:
        AddHTMLListItem(kType, "file", out);
        AddHTMLListItem(kPath,
                 net::EscapeForHTML(item.path().AsUTF8Unsafe()),
                 out);
        if (!item.expected_modification_time().is_null()) {
          AddHTMLListItem(kModificationTime, base::UTF16ToUTF8(
              TimeFormatFriendlyDateAndTime(item.expected_modification_time())),
              out);
        }
        break;
      case BlobDataItem::Type::kFileFilesystem:
        AddHTMLListItem(kType, "filesystem", out);
        AddHTMLListItem(kURL, item.filesystem_url().spec(), out);
        if (!item.expected_modification_time().is_null()) {
          AddHTMLListItem(kModificationTime, base::UTF16ToUTF8(
              TimeFormatFriendlyDateAndTime(item.expected_modification_time())),
              out);
        }
        break;
      case BlobDataItem::Type::kReadableDataHandle: {
        AddHTMLListItem(kType, "readable data handle", out);
        std::stringstream ss;
        item.data_handle()->PrintTo(&ss);
        AddHTMLListItem(kURL, ss.str(), out);
        break;
      }
      case BlobDataItem::Type::kBytesDescription:
        AddHTMLListItem(kType, "pending data", out);
        break;
    }
    if (item.offset()) {
      AddHTMLListItem(kOffset, base::UTF16ToUTF8(base::FormatNumber(
                                   static_cast<int64_t>(item.offset()))),
                      out);
    }
    if (static_cast<int64_t>(item.length()) != -1) {
      AddHTMLListItem(kLength, base::UTF16ToUTF8(base::FormatNumber(
                                   static_cast<int64_t>(item.length()))),
                      out);
    }

    if (has_multi_items)
      EndHTMLList(out);
  }

  EndHTMLList(out);
}

}  // namespace storage
