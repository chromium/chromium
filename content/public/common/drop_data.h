// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A struct for managing data being dropped on a WebContents.  This represents
// a union of all the types of data that can be dropped in a platform neutral
// way.

#ifndef CONTENT_PUBLIC_COMMON_DROP_DATA_H_
#define CONTENT_PUBLIC_COMMON_DROP_DATA_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT DropData {
  struct CONTENT_EXPORT FileSystemFileInfo {
    // Writes file system files to the pickle.
    static void WriteFileSystemFilesToPickle(
        const std::vector<FileSystemFileInfo>& file_system_files,
        base::Pickle* pickle);

    // Reads file system files from the pickle.
    static bool ReadFileSystemFilesFromPickle(
        const base::Pickle& pickle,
        std::vector<FileSystemFileInfo>* file_system_files);

    GURL url;
    int64_t size = 0;
    std::string filesystem_id;
  };

  enum class Kind {
    STRING = 0,
    FILENAME,
    FILESYSTEMFILE,
    BINARY,
    LAST = BINARY
  };

  struct Metadata {
    static Metadata CreateForMimeType(Kind kind,
                                      const std::u16string& mime_type);
    static Metadata CreateForFilePath(const base::FilePath& filename);
    static Metadata CreateForFileSystemUrl(const GURL& file_system_url);
    static Metadata CreateForBinary(const GURL& file_contents_url);

    Metadata();
    Metadata(const Metadata& other);
    ~Metadata();

    Kind kind;
    std::u16string mime_type;
    base::FilePath filename;
    GURL file_system_url;
    GURL file_contents_url;
  };

  DropData();
  DropData(const DropData& other);
  ~DropData();

  // Returns a sanitized filename to use for the dragged image, or std::nullopt
  // if no sanitized name could be synthesized.
  std::optional<base::FilePath> GetSafeFilenameForImageFileContents() const;

  int view_id = MSG_ROUTING_NONE;

  // Whether this drag originated from a renderer.
  bool did_originate_from_renderer = false;

  // Whether this drag is from a privileged WebContents.
  bool is_from_privileged = false;

  // User is dragging a link or image.
  GURL url;
  std::u16string url_title;  // The title associated with `url`.

  // User is dragging a link out-of the webview.
  std::u16string download_metadata;

  // Referrer policy to use when dragging a link out of the webview results in
  // a download.
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;

  // User is dropping one or more files on the webview. This field is only
  // populated if the drag is not renderer tainted, as this allows File access
  // from web content.
  std::vector<ui::FileInfo> filenames;
  // The mime types of dragged files.
  std::vector<std::u16string> file_mime_types;

  // Isolated filesystem ID for the files being dragged on the webview.
  std::u16string filesystem_id;

  // User is dragging files specified with filesystem: URLs.
  std::vector<FileSystemFileInfo> file_system_files;

  // User is dragging plain text into the webview.
  std::optional<std::u16string> text;

  // User is dragging text/html into the webview (e.g., out of Firefox).
  // `html_base_url` is the URL that the html fragment is taken from (used to
  // resolve relative links). It's ok for `html_base_url` to be empty.
  std::optional<std::u16string> html;
  GURL html_base_url;

  // User is dragging an image out of the WebView.
  std::string file_contents;
  bool file_contents_image_accessible = false;
  GURL file_contents_source_url;
  base::FilePath::StringType file_contents_filename_extension;
  std::string file_contents_content_disposition;

  std::unordered_map<std::u16string, std::u16string> custom_data;

  // The drop operation. See mojo method FrameWidget::DragTargetDragEnter() for
  // a discussion of `operation` and `document_is_handling_drag`.
  ui::mojom::DragOperation operation = ui::mojom::DragOperation::kNone;
  bool document_is_handling_drag = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_DROP_DATA_H_
