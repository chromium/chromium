// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIPBOARD_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_CLIPBOARD_TYPES_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;

// Structure of data pasted from clipboard, to be used by scanning code to
// determine if it should be allowed to be pasted.
struct CONTENT_EXPORT ClipboardPasteData {
  ClipboardPasteData();
  ClipboardPasteData(const ClipboardPasteData&);
  ClipboardPasteData& operator=(const ClipboardPasteData&);
  ClipboardPasteData(ClipboardPasteData&&);
  ClipboardPasteData& operator=(ClipboardPasteData&&);
  ~ClipboardPasteData();

  // Returns true if all the filds in the struct are null/empty.
  bool empty() const;

  // Returns the sum of the size of all fields except `file_paths`.
  // Since the meaning of that size could be ambiguous given the differences
  // between what fields actually represent, this should only be used when only
  // one field has been populated.
  size_t size() const;

  // Override the members of `this` with non-empty members in `other`. This is
  // used to merge `ClipboardPasteData` objects representing a single type of
  // clipboard data into one representing all of them.
  void Merge(ClipboardPasteData other);

  // UTF-16 encoded plain text data to scan.
  std::u16string text;

  // UTF-16 encoded HTML data to scan.
  std::u16string html;

  // UTF-16 encoded SVG data to scan.
  std::u16string svg;

  // UTF-8 encoded RTF data to scan.
  std::string rtf;

  // PNG bytes to scan.
  std::vector<uint8_t> png;

  // Bitmap to scan.
  SkBitmap bitmap;

  // A list of full file paths to scan.
  std::vector<base::FilePath> file_paths;

  // Custom data to scan, keyed by type.
  base::flat_map<std::u16string, std::u16string> custom_data;
};

// Class representing an endpoint tied to a clipboard interaction. This can
// represent either a source or destination.
class CONTENT_EXPORT ClipboardEndpoint {
 public:
  // This constructor should be called when the endpoint represents something
  // from outside of Chrome's control, such as copying from a different
  // application. On CrOS, `data_transfer_endpoint` might still be populated
  // with relevant information.
  explicit ClipboardEndpoint(base::optional_ref<const ui::DataTransferEndpoint>
                                 data_transfer_endpoint);

  // This constructor should be called when the endpoint represents a Chrome tab
  // that is still alive.
  ClipboardEndpoint(
      base::optional_ref<const ui::DataTransferEndpoint> data_transfer_endpoint,
      base::RepeatingCallback<BrowserContext*()> browser_context_fetcher,
      RenderFrameHost& rfh);

  ClipboardEndpoint(const ClipboardEndpoint&);
  ClipboardEndpoint& operator=(const ClipboardEndpoint&);

  ~ClipboardEndpoint();

  // `ui::DataTransferEndpoint` representation of the endpoint. This is empty in
  // some cases like copying from Chrome's omnibox, or copying from outside the
  // browser on non-CrOS platforms.
  const std::optional<ui::DataTransferEndpoint>& data_transfer_endpoint()
      const {
    return data_transfer_endpoint_;
  }

  // BrowserContext of a clipboard source/destination when it corresponds to a
  // browser tab. This can be null if the endpoint is not a Chrome tab, or if
  // the BrowserContext is gone when the object represents a clipboard source.
  BrowserContext* browser_context() const;

  // WebContents of a clipboard source/destination when it corresponds to a
  // browser tab. This can be null if the endpoint is not a Chrome tab, or if
  // the tab has been closed.
  WebContents* web_contents() const;

 private:
  // The `ui::DataTransferEndpoint` corresponding to the clipboard interaction.
  // An empty value represents a copy from Chrome's omnibox, a copy from a
  // different desktop application (outside of CrOS), etc.
  std::optional<ui::DataTransferEndpoint> data_transfer_endpoint_;

  // Fetcher method to provide a `BrowserContext` if the endpoint has one. This
  // is done so code that instantiates this class can bind a function with
  // proper lifetime management instead of storing a raw_ptr<BrowserContext>
  // that might eventually be dangling.
  base::RepeatingCallback<BrowserContext*()> browser_context_fetcher_;

  // null if the endpoint has no associated WebContents, or if it's been closed.
  base::WeakPtr<WebContents> web_contents_;
};

// Struct that holds metadata for data being copied or pasted that is relevant
// to evaluating enterprise policies.
struct ClipboardMetadata {
  // Size of the clipboard data. null when files are copied.
  std::optional<size_t> size;

  // Format type of clipboard data.
  ui::ClipboardFormatType format_type;

  // Sequence number of the clipboard interaction.
  ui::ClipboardSequenceNumberToken seqno;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIPBOARD_TYPES_H_
