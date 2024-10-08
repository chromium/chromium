// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_chooser.h"

#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/mime_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_elider.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

namespace {

// The maximum number of unicode code points the description of a file type is
// allowed to be. Any longer descriptions will be truncated to this length.
// The exact number here is fairly arbitrary, since font, font size, dialog
// size and underlying platform all influence how many characters will actually
// be visible. As such this can be adjusted as needed.
constexpr int kMaxDescriptionLength = 64;
// The maximum number of unicode code points the extension of a file is
// allowed to be. Any longer extensions will be stripped. This value should be
// kept in sync with the extension length checks in the renderer.
constexpr int kMaxExtensionLength = 16;

// Similar to base::FilePath::FinalExtension, but operates with the
// understanding that the StringType passed in is an extension, not a path.
// Returns the last extension without a leading ".".
base::FilePath::StringType GetLastExtension(
    const base::FilePath::StringType& extension) {
  auto last_separator = extension.rfind(base::FilePath::kExtensionSeparator);
  return (last_separator != base::FilePath::StringType::npos)
             ? extension.substr(last_separator + 1)
             : extension;
}

// Extension validation primarily takes place in the renderer. This checks for a
// subset of invalid extensions in the event the renderer is compromised.
bool IsInvalidExtension(base::FilePath::StringType& extension) {
  std::string component8 = base::FilePath(extension).AsUTF8Unsafe();
  auto extension16 = base::UTF8ToUTF16(component8);

  return !base::i18n::IsFilenameLegal(extension16) ||
         FileSystemChooser::IsShellIntegratedExtension(extension);
}

// Converts the accepted mime types and extensions from `option` into a list
// of just extensions to be passed to the file dialog implementation.
// The returned list will start with all the explicit website provided
// extensions in order, followed by (for each mime type) the preferred
// extension for that mime type (if any) and any other extensions associated
// with that mime type. Duplicates are filtered out so each extension only
// occurs once in the returned list.
bool GetFileTypesFromAcceptsOption(
    const blink::mojom::ChooseFileSystemEntryAcceptsOption& option,
    std::vector<base::FilePath::StringType>* extensions,
    std::u16string* description) {
  std::set<base::FilePath::StringType> extension_set;

  for (const std::string& extension_string : option.extensions) {
    base::FilePath::StringType extension;
#if BUILDFLAG(IS_WIN)
    extension = base::UTF8ToWide(extension_string);
#else
    extension = extension_string;
#endif
    if (extension_set.insert(extension).second &&
        !IsInvalidExtension(extension)) {
      extensions->push_back(std::move(extension));
    }
  }

  for (const std::string& mime_type : option.mime_types) {
    base::FilePath::StringType preferred_extension;
    if (net::GetPreferredExtensionForMimeType(mime_type,
                                              &preferred_extension)) {
      if (extension_set.insert(preferred_extension).second &&
          !IsInvalidExtension(preferred_extension)) {
        extensions->push_back(std::move(preferred_extension));
      }
    }

    std::vector<base::FilePath::StringType> inner;
    net::GetExtensionsForMimeType(mime_type, &inner);
    if (inner.empty())
      continue;
    for (auto& extension : inner) {
      if (extension_set.insert(extension).second &&
          !IsInvalidExtension(extension)) {
        extensions->push_back(std::move(extension));
      }
    }
  }

  if (extensions->empty())
    return false;

  std::u16string sanitized_description = option.description;
  if (!sanitized_description.empty()) {
    sanitized_description = base::CollapseWhitespace(
        sanitized_description, /*trim_sequences_with_line_breaks=*/false);
    sanitized_description = gfx::TruncateString(
        sanitized_description, kMaxDescriptionLength, gfx::CHARACTER_BREAK);
    base::i18n::SanitizeUserSuppliedString(&sanitized_description);
  }
  *description = sanitized_description;

  return true;
}

ui::SelectFileDialog::FileTypeInfo ConvertAcceptsToFileTypeInfo(
    const blink::mojom::AcceptsTypesInfoPtr& accepts_types_info) {
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.include_all_files = accepts_types_info->include_accepts_all;

  for (const auto& option : accepts_types_info->accepts) {
    std::vector<base::FilePath::StringType> extensions;
    std::u16string description;

    if (!GetFileTypesFromAcceptsOption(*option, &extensions, &description))
      continue;  // No extensions were found for this option, skip it.

    file_types.extensions.push_back(extensions);
    // FileTypeInfo expects each set of extension to have a corresponding
    // description. A blank description will result in a system generated
    // description to be used.
    file_types.extension_description_overrides.push_back(description);
  }

  if (file_types.extensions.empty())
    file_types.include_all_files = true;

  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  file_types.keep_extension_visible = true;

  return file_types;
}

ui::SelectFileDialog::Type ValidateType(ui::SelectFileDialog::Type type) {
  switch (type) {
    case ui::SelectFileDialog::SELECT_OPEN_FILE:
    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
    case ui::SelectFileDialog::SELECT_FOLDER:
      return type;
    default:
      NOTREACHED_IN_MIGRATION();
      return ui::SelectFileDialog::SELECT_NONE;
  }
}

}  // namespace

FileSystemChooser::Options::Options(
    ui::SelectFileDialog::Type type,
    blink::mojom::AcceptsTypesInfoPtr accepts_types_info,
    std::u16string title,
    base::FilePath default_directory,
    base::FilePath suggested_name)
    : type_(ValidateType(type)),
      file_types_(ConvertAcceptsToFileTypeInfo(accepts_types_info)),
      // Set `default_file_type_index_` to a reasonable default value.
      // This value will be updated if the extension of `suggested_name`
      // matches an extension in `accepts_types_info->accepts`.
      default_file_type_index_(file_types_.extensions.empty() ? 0 : 1),
      title_(std::move(title)),
      default_path_(default_directory.Append(
          ResolveSuggestedNameExtension(std::move(suggested_name),
                                        file_types_))) {}

FileSystemChooser::Options::Options(const Options& other) = default;

base::FilePath FileSystemChooser::Options::ResolveSuggestedNameExtension(
    base::FilePath suggested_name,
    ui::SelectFileDialog::FileTypeInfo& file_types) {
  if (suggested_name.empty())
    return base::FilePath();

  auto suggested_extension = suggested_name.Extension();

  if (suggested_extension.size() > kMaxExtensionLength) {
    // Sanitize extensions longer than 16 characters.
    file_types.include_all_files = true;
    return suggested_name.RemoveExtension();
  }

  if (file_types.extensions.empty() || suggested_extension.empty()) {
    file_types.include_all_files = true;
    return suggested_name;
  }

  // Strip leading ".".
  suggested_extension = suggested_extension.substr(1);

  // Check if the suggested extension is an accepted extension.
  for (auto i = 0u; i < file_types.extensions.size(); ++i) {
    auto it = base::ranges::find(file_types.extensions[i], suggested_extension);
    if (it != file_types.extensions[i].end()) {
      // The suggested extension is an accepted extension. All is harmonious.
      default_file_type_index_ = i + 1;  // NOTE: 1-based index.
      return suggested_name;
    }
  }

  // Suggested extension not found in non-empty `accepts`.
  file_types.include_all_files = true;
  return suggested_name;
}

// static
void FileSystemChooser::CreateAndShow(
    WebContents* web_contents,
    const Options& options,
    ResultCallback callback,
    base::ScopedClosureRunner fullscreen_block) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // `listener` deletes itself.
  auto* listener = new FileSystemChooser(options.type(), std::move(callback),
                                         std::move(fullscreen_block));
  listener->dialog_ = ui::SelectFileDialog::Create(
      listener,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));

  // In content_shell --run-web-tests, there might be no dialog available. In
  // that case just abort.
  if (!listener->dialog_) {
    listener->FileSelectionCanceled();
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  listener->dialog_->SetOpenWritable(true);
#endif
  listener->dialog_->SelectFile(
      options.type(), options.title(), options.default_path(),
      &options.file_type_info(), options.default_file_type_index(),
      /*default_extension=*/base::FilePath::StringType(),
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::NativeWindow(),
      /*caller=*/
      web_contents ? &web_contents->GetPrimaryMainFrame()->GetLastCommittedURL()
                   : nullptr);
}

// static
bool FileSystemChooser::IsShellIntegratedExtension(
    const base::FilePath::StringType& extension) {
  // TODO(crbug.com/40159607): Figure out some way to unify this with
  // net::IsSafePortablePathComponent, with the result probably ending up in
  // base/i18n/file_util_icu.h.
  // - For the sake of consistency across platforms, we sanitize '.lnk' and
  //   '.local' files on all platforms (not just Windows)
  // - There are some extensions (i.e. '.scf') we would like to sanitize which
  //   `net::GenerateFileName()` does not
  base::FilePath::StringType extension_lower =
      base::ToLowerASCII(GetLastExtension(extension));

  // '.lnk' and '.scf' files may be used to execute arbitrary code (see
  // https://nvd.nist.gov/vuln/detail/CVE-2010-2568 and
  // https://crbug.com/1227995, respectively). '.local' files are used by
  // Windows to determine which DLLs to load for an application. '.url' files
  // can be used to read arbirtary files (see https://crbug.com/1307930).
  if ((extension_lower == FILE_PATH_LITERAL("lnk")) ||
      (extension_lower == FILE_PATH_LITERAL("local")) ||
      (extension_lower == FILE_PATH_LITERAL("scf")) ||
      (extension_lower == FILE_PATH_LITERAL("url"))) {
    return true;
  }

  // Setting a file's extension to a CLSID may conceal its actual file type on
  // some Windows versions (see https://nvd.nist.gov/vuln/detail/CVE-2004-0420).
  if (!extension_lower.empty() &&
      (extension_lower.front() == FILE_PATH_LITERAL('{')) &&
      (extension_lower.back() == FILE_PATH_LITERAL('}'))) {
    return true;
  }

  return false;
}

FileSystemChooser::FileSystemChooser(ui::SelectFileDialog::Type type,
                                     ResultCallback callback,
                                     base::ScopedClosureRunner fullscreen_block)
    : callback_(std::move(callback)),
      type_(ValidateType(type)),
      fullscreen_block_(std::move(fullscreen_block)) {}

FileSystemChooser::~FileSystemChooser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dialog_)
    dialog_->ListenerDestroyed();
}

void FileSystemChooser::FileSelected(const ui::SelectedFileInfo& file,
                                     int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MultiFilesSelected({file});
}

void FileSystemChooser::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PathInfo> result;

  for (const ui::SelectedFileInfo& file : files) {
    if (file.virtual_path.has_value()) {
      base::FilePath display_name = !file.display_name.empty()
                                        ? base::FilePath(file.display_name)
                                        : file.virtual_path->BaseName();
      result.emplace_back(PathType::kExternal, *file.virtual_path,
                          display_name.AsUTF8Unsafe());
    } else {
      base::FilePath path =
          !file.local_path.empty() ? file.local_path : file.file_path;
      base::FilePath display_name = !file.display_name.empty()
                                        ? base::FilePath(file.display_name)
                                        : path.BaseName();
      result.emplace_back(PathType::kLocal, std::move(path),
                          display_name.AsUTF8Unsafe());
    }
  }

  std::move(callback_).Run(file_system_access_error::Ok(), std::move(result));
  delete this;
}

void FileSystemChooser::FileSelectionCanceled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(
      file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kOperationAborted),
      {});
  delete this;
}

}  // namespace content
