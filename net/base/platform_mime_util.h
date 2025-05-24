// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PLATFORM_MIME_UTIL_H_
#define NET_BASE_PLATFORM_MIME_UTIL_H_

#include <string>
#include <string_view>
#include <unordered_set>

#include "base/files/file_path.h"

namespace net {

// Encapsulates the platform-specific functionality in mime_util.
class PlatformMimeUtil {
 public:
  // Adds all the extensions that the platform associates with the type
  // |mime_type| to the set |extensions|.  Returns at least the value returned
  // by GetPreferredExtensionForMimeType.
  void GetPlatformExtensionsForMimeType(
      std::string_view mime_type,
      std::unordered_set<base::FilePath::StringType>* extensions) const;

 protected:
  // Gets the preferred filename extension associated with the given
  // mime type. Returns true if the file type is registered in the system. The
  // extension is returned without a prefixed dot, ex "html".
  bool GetPlatformPreferredExtensionForMimeType(
      std::string_view mime_type,
      base::FilePath::StringType* extension) const;

  // Gets the mime type (if any) that is associated with the file extension.
  // Returns true if a corresponding mime type exists.
  bool GetPlatformMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                        std::string* mime_type) const;
};

}  // namespace net

#endif  // NET_BASE_PLATFORM_MIME_UTIL_H_
