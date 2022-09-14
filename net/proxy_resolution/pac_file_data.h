// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PAC_FILE_DATA_H_
#define NET_PROXY_RESOLUTION_PAC_FILE_DATA_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net {

// Reference-counted wrapper for passing around a PAC script specification.
// The PAC script can be either specified via a URL, a deferred URL for
// auto-detect, or the actual javascript program text.
//
// This is thread-safe so it can be used by multi-threaded implementations of
// ProxyResolver to share the data between threads.
class NET_EXPORT_PRIVATE PacFileData
    : public base::RefCountedThreadSafe<PacFileData> {
 public:
  enum Type {
    TYPE_SCRIPT_CONTENTS,
    TYPE_SCRIPT_URL,
    TYPE_AUTO_DETECT,
  };

  // Creates a script data given the UTF8 bytes of the content.
  static scoped_refptr<PacFileData> FromUTF8(const std::string& utf8);

  // Creates a script data given the UTF16 bytes of the content.
  static scoped_refptr<PacFileData> FromUTF16(const std::u16string& utf16);

  // Creates a script data given a URL to the PAC script.
  static scoped_refptr<PacFileData> FromURL(const GURL& url);

  // Creates a script data for using an automatically detected PAC URL.
  static scoped_refptr<PacFileData> ForAutoDetect();

  Type type() const { return type_; }

  // Returns the contents of the script as UTF16.
  // (only valid for type() == TYPE_SCRIPT_CONTENTS).
  const std::u16string& utf16() const;

  // Returns the URL of the script.
  // (only valid for type() == TYPE_SCRIPT_URL).
  const GURL& url() const;

  // Returns true if |this| matches |other|.
  bool Equals(const PacFileData* other) const;

 private:
  friend class base::RefCountedThreadSafe<PacFileData>;
  PacFileData(Type type, const GURL& url, const std::u16string& utf16);
  virtual ~PacFileData();

  const Type type_;
  const GURL url_;
  const std::u16string utf16_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PAC_FILE_DATA_H_
