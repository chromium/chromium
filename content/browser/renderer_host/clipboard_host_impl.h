// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace ui {
class ScopedClipboardWriter;
}  // namespace ui

namespace content {

class ClipboardHostImplTest;

// Helpers to check if an `rfh`/`seqno` pair was the last to write to the
// clipboard.
bool IsLastClipboardWrite(const RenderFrameHost& rfh,
                          ui::ClipboardSequenceNumberToken seqno);

// Helper to set the last rfh-seqno pair that wrote to the clipboard.
void SetLastClipboardWrite(const RenderFrameHost& rfh,
                           ui::ClipboardSequenceNumberToken seqno);

// Returns a representation of the last source ClipboardEndpoint. This will
// either match the last clipboard write if `seqno` matches the last browser tab
// write, or an endpoint built from `Clipboard::GetSource()` called with
// `clipboard_buffer` otherwise.
//
// //content maintains additional metadata on top of what the //ui layer already
// tracks about clipboard data's source, e.g. the WebContents that provided the
// data. This function allows retrieving both the //ui metadata and the
// //content metadata in a single call.
//
// To avoid returning stale //content metadata if the writer has changed, the
// sequence number is used to validate if the writer has changed or not since
// the //content metadata was last updated.
CONTENT_EXPORT ClipboardEndpoint
GetSourceClipboardEndpoint(ui::ClipboardSequenceNumberToken seqno,
                           ui::ClipboardBuffer clipboard_buffer);

class CONTENT_EXPORT ClipboardHostImpl
    : public DocumentService<blink::mojom::ClipboardHost> {
 public:
  ~ClipboardHostImpl() override;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  using ClipboardPasteData = content::ClipboardPasteData;

 protected:
  // These types and methods are protected for testing.

  using IsClipboardPasteAllowedCallback =
      RenderFrameHostImpl::IsClipboardPasteAllowedCallback;

  explicit ClipboardHostImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  // Performs a check to see if pasting `data` is allowed by data transfer
  // policies and invokes FinishPasteIfAllowed upon completion.
  void PasteIfPolicyAllowed(ui::ClipboardBuffer clipboard_buffer,
                            const ui::ClipboardFormatType& data_type,
                            ClipboardPasteData clipboard_paste_data,
                            IsClipboardPasteAllowedCallback callback);

 private:
  friend class ClipboardHostImplTest;
  friend class ClipboardHostImplWriteTest;
  friend class ClipboardHostImplAsyncWriteTest;

  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteText);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteText_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteHtml_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteSvg);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteSvg_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBitmap);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBitmap_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           WriteDataTransferCustomData);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           WriteDataTransferCustomData_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           PerformPasteIfAllowed_EmptyData);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, MainFrameURL);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, GetSourceEndpoint);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteText);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteTextAndHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, ConcurrentWrites);

  // mojom::ClipboardHost
  void GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(blink::mojom::ClipboardFormat format,
                         ui::ClipboardBuffer clipboard_buffer,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(ui::ClipboardBuffer clipboard_buffer,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(ui::ClipboardBuffer clipboard_buffer,
                ReadTextCallback callback) override;
  void ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                ReadHtmlCallback callback) override;
  void ReadSvg(ui::ClipboardBuffer clipboard_buffer,
               ReadSvgCallback callback) override;
  void ReadRtf(ui::ClipboardBuffer clipboard_buffer,
               ReadRtfCallback callback) override;
  void ReadPng(ui::ClipboardBuffer clipboard_buffer,
               ReadPngCallback callback) override;
  void ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                 ReadFilesCallback callback) override;
  void ReadDataTransferCustomData(
      ui::ClipboardBuffer clipboard_buffer,
      const std::u16string& type,
      ReadDataTransferCustomDataCallback callback) override;
  void ReadAvailableCustomAndStandardFormats(
      ReadAvailableCustomAndStandardFormatsCallback callback) override;
  void ReadUnsanitizedCustomFormat(
      const std::u16string& format,
      ReadUnsanitizedCustomFormatCallback callback) override;
  void WriteUnsanitizedCustomFormat(const std::u16string& format,
                                    mojo_base::BigBuffer data) override;
  void WriteText(const std::u16string& text) override;
  void WriteHtml(const std::u16string& markup, const GURL& url) override;
  void WriteSvg(const std::u16string& markup) override;
  void WriteSmartPasteMarker() override;
  void WriteDataTransferCustomData(
      const base::flat_map<std::u16string, std::u16string>& data) override;
  void WriteBookmark(const std::string& url,
                     const std::u16string& title) override;
  void WriteImage(const SkBitmap& unsafe_bitmap) override;
  void CommitWrite() override;
#if BUILDFLAG(IS_MAC)
  void WriteStringToFindPboard(const std::u16string& text) override;
#endif

  // Checks if the renderer allows pasting.  This check is skipped if called
  // soon after a successful content allowed request.
  bool IsRendererPasteAllowed(ui::ClipboardBuffer clipboard_buffer,
                              RenderFrameHost& render_frame_host);

  // Helper to be used when checking if data is allowed to be copied.
  //
  // If `replacement_data` is null, `clipboard_writer_` will be used to write
  // `data` to the clipboard. `data` should only have one of its fields set
  // depending on which "Write" method lead to `OnCopyAllowedResult()` being
  // called. That field should correspond to `data_type`.
  //
  // If `replacement_data` is not null, instead that replacement string is
  // written to the clipboard as plaintext.
  //
  // This method can be called asynchronously.
  virtual void OnCopyAllowedResult(
      const ui::ClipboardFormatType& data_type,
      const ClipboardPasteData& data,
      std::optional<std::u16string> replacement_data);

  // Does the same thing as the previous function with an extra `source_url`
  // used to propagate the URL obtained in the `WriteHtml()` method call.
  //
  // This method can be called asynchronously.
  virtual void OnCopyHtmlAllowedResult(
      const GURL& source_url,
      const ui::ClipboardFormatType& data_type,
      const ClipboardPasteData& data,
      std::optional<std::u16string> replacement_data);

  using CopyAllowedCallback = base::OnceCallback<void()>;

  void OnReadPng(ui::ClipboardBuffer clipboard_buffer,
                 ReadPngCallback callback,
                 const std::vector<uint8_t>& data);

  // Creates a `ui::DataTransferEndpoint` representing the last committed URL.
  std::unique_ptr<ui::DataTransferEndpoint> CreateDataEndpoint();

  // Creates a `content::ClipboardEndpoint` representing the last committed URL.
  ClipboardEndpoint CreateClipboardEndpoint();

  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;

  // Counts the number of expected `Write*` calls to be made to the current
  // `clipboard_writer_`. This should be used to handle asynchronous `Write*`
  // calls made by `IsClipboardCopyAllowedByPolicy`.
  int pending_writes_ = 0;

  // Indicates that the renderer called `CommitWrite()`, but that
  // `pending_writes_` was not 0 at that time and that it should instead be
  // called when the last pending `Write*` call is made.
  bool pending_commit_write_ = false;

  base::WeakPtrFactory<ClipboardHostImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
