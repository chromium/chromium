// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/pac_file_data.h"

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"

namespace net {

// static
scoped_refptr<PacFileData> PacFileData::FromUTF8(const std::string& utf8) {
  return base::WrapRefCounted(
      new PacFileData(TYPE_SCRIPT_CONTENTS, GURL(), base::UTF8ToUTF16(utf8)));
}

// static
scoped_refptr<PacFileData> PacFileData::FromUTF16(const std::u16string& utf16) {
  return base::WrapRefCounted(
      new PacFileData(TYPE_SCRIPT_CONTENTS, GURL(), utf16));
}

// static
scoped_refptr<PacFileData> PacFileData::FromURL(const GURL& url) {
  return base::WrapRefCounted(
      new PacFileData(TYPE_SCRIPT_URL, url, std::u16string()));
}

// static
scoped_refptr<PacFileData> PacFileData::ForAutoDetect() {
  return base::WrapRefCounted(
      new PacFileData(TYPE_AUTO_DETECT, GURL(), std::u16string()));
}

const std::u16string& PacFileData::utf16() const {
  DCHECK_EQ(TYPE_SCRIPT_CONTENTS, type_);
  return utf16_;
}

const GURL& PacFileData::url() const {
  DCHECK_EQ(TYPE_SCRIPT_URL, type_);
  return url_;
}

bool PacFileData::Equals(const PacFileData* other) const {
  if (type() != other->type())
    return false;

  switch (type()) {
    case TYPE_SCRIPT_CONTENTS:
      return utf16() == other->utf16();
    case TYPE_SCRIPT_URL:
      return url() == other->url();
    case TYPE_AUTO_DETECT:
      return true;
  }

  return false;  // Shouldn't be reached.
}

PacFileData::PacFileData(Type type,
                         const GURL& url,
                         const std::u16string& utf16)
    : type_(type), url_(url), utf16_(utf16) {}

PacFileData::~PacFileData() = default;

}  // namespace net
