// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/path_util.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#endif

namespace extensions {
namespace path_util {

namespace {
#if BUILDFLAG(IS_MAC)

// Retrieves the localized display name for the base name of the given path.
// If the path is not localized, this will just return the base name.
std::string GetDisplayBaseName(const base::FilePath& path) {
  base::apple::ScopedCFTypeRef<CFURLRef> url =
      base::apple::FilePathToCFURL(path);
  if (!url) {
    return path.BaseName().value();
  }

  base::apple::ScopedCFTypeRef<CFStringRef> str;
  if (!CFURLCopyResourcePropertyForKey(url.get(), kCFURLLocalizedNameKey,
                                       str.InitializeInto(),
                                       /*error=*/nullptr)) {
    return path.BaseName().value();
  }

  return base::SysCFStringRefToUTF8(str.get());
}

#endif  // BUILDFLAG(IS_MAC)

const base::FilePath::CharType kHomeShortcut[] = FILE_PATH_LITERAL("~");

void OnDirectorySizeCalculated(
    int message_id,
    base::OnceCallback<void(const std::u16string&)> callback,
    int64_t size_in_bytes) {
  const int one_mebibyte_in_bytes = 1024 * 1024;
  std::u16string response =
      size_in_bytes < one_mebibyte_in_bytes
          ? l10n_util::GetStringUTF16(message_id)
          : ui::FormatBytesWithUnits(size_in_bytes, ui::DATA_UNITS_MEBIBYTE,
                                     true);

  std::move(callback).Run(response);
}

}  // namespace

base::FilePath PrettifyPath(const base::FilePath& source_path) {
  base::FilePath home_path;
  if (source_path.empty() ||
      !base::PathService::Get(base::DIR_HOME, &home_path)) {
    return source_path;
  }

  base::FilePath display_path = base::FilePath(kHomeShortcut);
  if (source_path == home_path) {
    return display_path;
  }

#if BUILDFLAG(IS_MAC)
  DCHECK(source_path.IsAbsolute());

  // Break down the incoming path into components, and grab the display name
  // for every component. This will match app bundles, ".localized" folders,
  // and localized subfolders of the user's home directory.
  // Don't grab the display name of the first component, i.e., "/", as it'll
  // show up as the HDD name.
  std::vector<base::FilePath::StringType> components =
      source_path.GetComponents();
  display_path = base::FilePath(components[0]);
  base::FilePath actual_path = display_path;
  for (std::vector<base::FilePath::StringType>::iterator i =
           components.begin() + 1; i != components.end(); ++i) {
    actual_path = actual_path.Append(*i);
    if (actual_path == home_path) {
      display_path = base::FilePath(kHomeShortcut);
      home_path = base::FilePath();
      continue;
    }
    std::string display = GetDisplayBaseName(actual_path);
    display_path = display_path.Append(display);
  }
  DCHECK_EQ(actual_path.value(), source_path.value());
  return display_path;
#else   // BUILDFLAG(IS_MAC)
  if (home_path.AppendRelativePath(source_path, &display_path)) {
    return display_path;
  }
  return source_path;
#endif  // BUILDFLAG(IS_MAC)
}

void CalculateExtensionDirectorySize(
    const base::FilePath& extension_path,
    base::OnceCallback<void(const int64_t)> callback) {
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::ComputeDirectorySize, extension_path),
      std::move(callback));
}

void CalculateAndFormatExtensionDirectorySize(
    const base::FilePath& extension_path,
    int message_id,
    base::OnceCallback<void(const std::u16string&)> callback) {
  CalculateExtensionDirectorySize(
      extension_path, base::BindOnce(&OnDirectorySizeCalculated, message_id,
                                     std::move(callback)));
}

base::FilePath ResolveHomeDirectory(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  return path;
#else
  const auto& value = path.value();
  // Look for a path starting with the "~" character. It must be alone or
  // followed by a separator.
  if (value.empty() || value[0] != FILE_PATH_LITERAL('~') ||
      (value.length() > 1 && !base::FilePath::IsSeparator(value[1]))) {
    return path;
  }
  base::FilePath result;
  base::PathService::Get(base::DIR_HOME, &result);
  // The user could specify "~" or "~/", so be safe.
  if (value.length() > 2) {
    result = result.Append(value.substr(2));
  }
  return result;
#endif
}

}  // namespace path_util
}  // namespace extensions
