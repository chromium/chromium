// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions used internally by filename_util, and filename_util_icu.

#ifndef NET_BASE_FILENAME_UTIL_INTERNAL_H_
#define NET_BASE_FILENAME_UTIL_INTERNAL_H_

#include <string>

#include "base/files/file_path.h"

class GURL;

namespace net {

using ReplaceIllegalCharactersFunction =
    void (*)(base::FilePath::StringType* file_name, char replace_char);

void SanitizeGeneratedFileName(base::FilePath::StringType* filename,
                               bool replace_trailing);

bool IsShellIntegratedExtension(const base::FilePath::StringType& extension);

void EnsureSafeExtension(const std::string& mime_type,
                         bool ignore_extension,
                         base::FilePath* file_name);

bool FilePathToString16(const base::FilePath& path, std::u16string* converted);

// Similar to GetSuggestedFilename(), but takes a function to replace illegal
// characters. If |should_replace_extension| is true, the file extension
// extracted from a URL will always be considered unreliable and the file
// extension will be determined by |mime_type|.
std::u16string GetSuggestedFilenameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name,
    bool should_replace_extension,
    ReplaceIllegalCharactersFunction replace_illegal_characters_function);

// Similar to GenerateFileName(), but takes a function to replace illegal
// characters. If |should_replace_extension| is true, the file extension
// extracted from a URL will always be considered unreliable and the file
// extension will be determined by |mime_type|.
base::FilePath GenerateFileNameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name,
    bool should_replace_extension,
    ReplaceIllegalCharactersFunction replace_illegal_characters_function);

}  // namespace net

#endif  // NET_BASE_FILENAME_UTIL_INTERNAL_H_
