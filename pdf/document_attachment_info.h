// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_ATTACHMENT_INFO_H_
#define PDF_DOCUMENT_ATTACHMENT_INFO_H_

#include <string>

#include "base/strings/string16.h"

namespace chrome_pdf {

struct DocumentAttachmentInfo {
  DocumentAttachmentInfo();

  DocumentAttachmentInfo(const DocumentAttachmentInfo& other);

  ~DocumentAttachmentInfo();

  // The attachment's name.
  base::string16 name;

  // The attachment's size in bytes.
  uint32_t size_bytes = 0;

  // The creation date of the attachment. It stores the arbitrary string saved
  // in field "CreationDate".
  base::string16 creation_date;

  // Last modified date of the attachment. It stores the arbitrary string saved
  // in field "ModDate".
  base::string16 modified_date;

  // The flag that indicates whether the attachment can be retrieved
  // successfully.
  bool is_readable = false;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_ATTACHMENT_INFO_H_
