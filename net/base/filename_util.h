// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FILENAME_UTIL_H_
#define NET_BASE_FILENAME_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "net/base/net_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace  net {

// Given the full path to a file name, creates a file: URL. The returned URL
// may not be valid if the input is malformed.
NET_EXPORT GURL FilePathToFileURL(const base::FilePath& path);

// Converts a file: URL back to a filename that can be passed to the OS. The
// file URL must be well-formed (GURL::is_valid() must return true); we don't
// handle degenerate cases here. Returns true on success, false if |url| is
// invalid or the file path cannot be extracted from |url|.
// On failure, *file_path will be empty.
//
// Do not call this with a |url| that doesn't have a file:// scheme.
// The implementation is specific to the platform filesystem, and not
// applicable to other schemes.
NET_EXPORT bool FileURLToFilePath(const GURL& url, base::FilePath* file_path);

// Generates a filename using the first successful method from the following (in
// order):
//
// 1) The raw Content-Disposition header in |content_disposition| as read from
//    the network.  |referrer_charset| is used to decode non-ASCII strings.
// 2) |suggested_name| if specified.  |suggested_name| is assumed to be in
//    UTF-8.
// 3) The filename extracted from the |url|.  |referrer_charset| will be used to
//    interpret the URL if there are non-ascii characters.The file extension for
//    filenames extracted from the URL are considered unreliable if the URL
//    contains a query string. If a MIME type is available (i.e. |mime_type| is
//    not empty) and that MIME type has a preferred extension, then the
//    resulting filename will have that preferred extension.
// 4) |default_name|.  If non-empty, |default_name| is assumed to be a filename
//    and shouldn't contain a path.  |default_name| is not subject to validation
//    or sanitization, and therefore shouldn't be a user supplied string.
// 5) The hostname portion from the |url|
//
// Then, leading and trailing '.'s will be removed.  On Windows, trailing spaces
// are also removed.  The string "download" is the final fallback if no filename
// is found or the filename is empty.
//
// Any illegal characters in the filename will be replaced by '-'.  If the
// filename doesn't contain an extension, and a |mime_type| is specified, the
// preferred extension for the |mime_type| will be appended to the filename.
// The resulting filename is then checked against a list of reserved names on
// Windows.  If the name is reserved, an underscore will be prepended to the
// filename.
//
// Note: |mime_type| should only be specified if this function is called from a
// thread that allows IO.
NET_EXPORT base::string16 GetSuggestedFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name);

// Similar to GetSuggestedFilename(), but returns a FilePath.
NET_EXPORT base::FilePath GenerateFileName(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name);

// Similar to GetSuggestedFilename(). If |should_replace_extension| is true, the
// file extension extracted from a URL will always be considered unreliable and
// the file extension will be determined by |mime_type|.
NET_EXPORT base::FilePath GenerateFileName(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name,
    bool should_replace_extension);

// Valid components:
// * are not empty
// * are not Windows reserved names (CON, NUL.zip, etc.)
// * do not have trailing separators
// * do not equal kCurrentDirectory
// * do not reference the parent directory
// * do not contain illegal characters
// * do not end with Windows shell-integrated extensions (even on posix)
// * do not begin with '.' (which would hide them in most file managers)
// * do not end with ' ' or '.'
NET_EXPORT bool IsSafePortablePathComponent(const base::FilePath& component);

// Basenames of valid relative paths are IsSafePortableBasename(), and internal
// path components of valid relative paths are valid path components as
// described above IsSafePortableBasename(). Valid relative paths are not
// absolute paths.
NET_EXPORT bool IsSafePortableRelativePath(const base::FilePath& path);

// Ensures that the filename and extension is safe to use in the filesystem.
//
// Assumes that |file_path| already contains a valid path or file name.  On
// Windows if the extension causes the file to have an unsafe interaction with
// the shell (see net_util::IsShellIntegratedExtension()), then it will be
// replaced by the string 'download'.  If |file_path| doesn't contain an
// extension or |ignore_extension| is true then the preferred extension, if one
// exists, for |mime_type| will be used as the extension.
//
// On Windows, the filename will be checked against a set of reserved names, and
// if so, an underscore will be prepended to the name.
//
// |file_name| can either be just the file name or it can be a full path to a
// file.
//
// Note: |mime_type| should only be non-empty if this function is called from a
// thread that allows IO.
NET_EXPORT void GenerateSafeFileName(const std::string& mime_type,
                                     bool ignore_extension,
                                     base::FilePath* file_path);

// Returns whether the specified file name is a reserved name on Windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the Windows shell.
// Even on other platforms, this will return whether or not a file name is
// reserved on Windows.
NET_EXPORT bool IsReservedNameOnWindows(
    const base::FilePath::StringType& filename);

}  // namespace net

#endif  // NET_BASE_FILENAME_UTIL_H_
