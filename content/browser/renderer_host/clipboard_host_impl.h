// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/child_process_id.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_observer.h"

class GURL;

namespace blink {
class StorageKey;
}  // namespace blink

namespace ui {
class ScopedClipboardWriter;
struct ClipboardMetadata;
}  // namespace ui

namespace content {

class BrowserContext;
class ClipboardHostImplTest;
class RenderFrameHost;
class ServiceWorkerHost;
class StoragePartitionImpl;

class CONTENT_EXPORT ClipboardHostImpl : public blink::mojom::ClipboardHost,
                                         public ui::ClipboardObserver {
 public:
  using ClipboardPasteData = content::ClipboardPasteData;
  using IsClipboardPasteAllowedCallback =
      ContentBrowserClient::IsClipboardPasteAllowedCallback;

  // The execution context a host serves, a document or a service worker. The
  // host body reaches its context only through this, so the two differ only
  // in what they answer here.
  class CONTENT_EXPORT Context {
   public:
    virtual ~Context() = default;

    // False for a document that may not use the clipboard right now and for a
    // worker that is not running. A document that is in the back/forward cache
    // or prerendering is evicted or cancelled by the question, which is why
    // every request starts here and OnClipboardDataChanged() does not.
    virtual bool IsActive() = 0;

    // Whether the document is active, asked on every clipboard change while a
    // listener is registered. No side effects, unlike IsActive().
    virtual bool CanObserveChanges() = 0;

    // A worker only writes. It has no focus or visibility to read with, and
    // the Data Controls type replacement is keyed on a frame.
    virtual bool CanRead() = 0;

    // The embedder's per-request paste check: for a document, transient user
    // activation or the clipboard-read permission.
    virtual bool IsPasteAllowed() = 0;

    // The embedder's per-request write check. A document has none.
    virtual bool CanWrite() = 0;

    virtual BrowserContext* GetBrowserContext() = 0;
    virtual StoragePartitionImpl* GetStoragePartition() = 0;
    virtual ChildProcessId GetChildProcessId() = 0;
    virtual blink::StorageKey GetStorageKey() = 0;

    virtual std::optional<ui::DataTransferEndpoint> CreateDataEndpoint() = 0;
    virtual ClipboardEndpoint CreateClipboardEndpoint() = 0;
    virtual void AddSourceDataToClipboardWriter(
        ui::ScopedClipboardWriter& clipboard_writer) = 0;

    // The enterprise policy hooks a document forwards to its WebContents. A
    // worker has no tab: no replaced types, every paste denied, no copy
    // notification.
    virtual std::optional<std::vector<std::u16string>>
    GetClipboardTypesIfPolicyApplied(
        const ui::ClipboardSequenceNumberToken& seqno) = 0;
    virtual void IsClipboardPasteAllowedByPolicy(
        const ClipboardEndpoint& source,
        const ClipboardEndpoint& destination,
        const ui::ClipboardMetadata& metadata,
        ClipboardPasteData clipboard_paste_data,
        IsClipboardPasteAllowedCallback callback) = 0;
    virtual void OnTextCopiedToClipboard(const std::u16string& copied_text) = 0;

#if BUILDFLAG(IS_CHROMEOS)
    // The Files app pastes its own custom formats alongside files, where a
    // web page gets only the file list.
    virtual bool IncludeAllTypesWhenFilesPresent() = 0;
#endif
  };

  explicit ClipboardHostImpl(std::unique_ptr<Context> context);
  explicit ClipboardHostImpl(RenderFrameHost& render_frame_host);
  explicit ClipboardHostImpl(ServiceWorkerHost& service_worker_host);
  ~ClipboardHostImpl() override;

  // Override for ui::ClipboardObserver
  void OnClipboardDataChanged() override;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  // mojom::ClipboardHost
  void RegisterClipboardListener(
      mojo::PendingRemote<blink::mojom::ClipboardListener> listener) override;
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
  void GetPlatformPermissionState(
      GetPlatformPermissionStateCallback callback) override;
#endif

 protected:
  // Protected for testing.
  //
  // Performs a check to see if pasting `data` is allowed by data transfer
  // policies and invokes FinishPasteIfAllowed upon completion.
  void PasteIfPolicyAllowed(ui::ClipboardBuffer clipboard_buffer,
                            const ui::ClipboardFormatType& data_type,
                            ui::ClipboardSequenceNumberToken seqno,
                            ClipboardPasteData clipboard_paste_data,
                            IsClipboardPasteAllowedCallback callback);

 private:
  friend class ClipboardHostImplTest;
  friend class ClipboardHostImplWriteTest;
  friend class ClipboardHostImplAsyncWriteTest;
  friend class ClipboardHostImplChangeTest;

  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteText);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteText_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteHtml_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteSvg);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteSvg_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBookmark_ValidUrl);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           WriteBookmark_InvalidUrl_DoesNotCrash);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBookmark_EmptyUrl);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBookmark_FileUrl);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBitmap);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, WriteBitmap_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           WriteDataTransferCustomData);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           WriteDataTransferCustomData_Empty);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           PerformPasteIfAllowed_EmptyData);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest,
                           NoSourceWithoutDataWrite);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, MainFrameURL);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplWriteTest, GetSourceEndpoint);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteText);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, WriteTextAndHtml);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplAsyncWriteTest, ConcurrentWrites);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplChangeTest, AddClipboardListener);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplChangeTest,
                           ClipboardListenerDisconnect);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplChangeTest,
                           NoNotificationToInactiveDocument);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplChangeTest,
                           NoNotificationWhenDocumentBecomesInactiveDuringRead);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplChangeTest,
                           NoNotificationWhenListenerDisconnectsDuringRead);

  absl::uint128 GetSequenceNumberImpl(ui::ClipboardBuffer clipboard_buffer);

  bool IsPasteAllowed();
  bool IsWriteAllowed();

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

  // Does the same thing as the previous functions but for custom formats.
  // The raw binary `data` is written to the clipboard using the specified
  // `format`.
  //
  // This method can be called asynchronously.
  virtual void OnCopyCustomFormatAllowedResult(
      const std::u16string& format,
      mojo_base::BigBuffer data,
      const ui::ClipboardFormatType& data_type,
      const ClipboardPasteData& paste_data,
      std::optional<std::u16string> replacement_data);

  using CopyAllowedCallback = base::OnceCallback<void()>;

  void OnReadAvailableTypes(ui::ClipboardBuffer clipboard_buffer,
                            ReadAvailableTypesCallback callback,
                            std::vector<std::u16string> types);

  void OnGetAllAvailableFormatsForReadAvailableTypes(
      ui::ClipboardBuffer clipboard_buffer,
      std::optional<ui::DataTransferEndpoint> data_dst,
      ReadAvailableTypesCallback callback,
      base::flat_set<ui::ClipboardFormatType> formats);

  void OnReadPng(ui::ClipboardBuffer clipboard_buffer,
                 ui::ClipboardSequenceNumberToken seqno,
                 ReadPngCallback callback,
                 const std::vector<uint8_t>& data);

  void OnReadPngWithText(ui::ClipboardBuffer clipboard_buffer,
                         ui::ClipboardSequenceNumberToken seqno,
                         ReadPngCallback callback,
                         std::vector<uint8_t> data,
                         std::u16string text);

  void OnReadText(ui::ClipboardBuffer clipboard_buffer,
                  ui::ClipboardSequenceNumberToken seqno,
                  ReadTextCallback callback,
                  std::u16string text);

  void OnReadHtml(ui::ClipboardBuffer clipboard_buffer,
                  ui::ClipboardSequenceNumberToken seqno,
                  ReadHtmlCallback callback,
                  std::u16string markup,
                  GURL src_url,
                  uint32_t fragment_start,
                  uint32_t fragment_end);

  void OnReadSvg(ui::ClipboardBuffer clipboard_buffer,
                 ui::ClipboardSequenceNumberToken seqno,
                 ReadSvgCallback callback,
                 std::u16string svg);

  void OnReadRtf(ui::ClipboardBuffer clipboard_buffer,
                 ui::ClipboardSequenceNumberToken seqno,
                 ReadRtfCallback callback,
                 std::string rtf);

  void OnReadFiles(ui::ClipboardBuffer clipboard_buffer,
                   ui::ClipboardSequenceNumberToken seqno,
                   ReadFilesCallback callback,
                   std::vector<ui::FileInfo> filenames);

  // Completes ReadFiles() once the data controls / DLP policy decision is
  // available. Grants the renderer read access to only the files the policy
  // allows, so no capability is ever issued for blocked files.
  void OnReadFilesPolicyResult(
      std::vector<ui::FileInfo> filenames,
      ReadFilesCallback callback,
      std::optional<ClipboardPasteData> clipboard_paste_data);

  void OnReadDataTransferCustomData(ui::ClipboardBuffer clipboard_buffer,
                                    const std::u16string& type,
                                    ui::ClipboardSequenceNumberToken seqno,
                                    ReadDataTransferCustomDataCallback callback,
                                    std::u16string data);

  void OnGetSourceClipboardEndpoint(const ui::ClipboardFormatType& data_type,
                                    ClipboardPasteData clipboard_paste_data,
                                    IsClipboardPasteAllowedCallback callback,
                                    std::optional<size_t> data_size,
                                    ui::ClipboardSequenceNumberToken seqno,
                                    content::ClipboardEndpoint data_dst,
                                    content::ClipboardEndpoint source);

  void OnReadUnsanitizedCustomFormat(
      ui::ClipboardSequenceNumberToken seqno,
      ReadUnsanitizedCustomFormatCallback callback,
      std::string data);

  void OnExtractCustomPlatformNames(
      const std::string& format_name,
      std::optional<ui::DataTransferEndpoint> data_endpoint,
      ui::ClipboardSequenceNumberToken seqno,
      ReadUnsanitizedCustomFormatCallback callback,
      std::map<std::string, std::string> custom_format_names);

  bool CanSendClipboardChangeNotification() const;

  void OnReadAvailableTypesForUpdate(absl::uint128 change_id,
                                     std::vector<std::u16string> types);

  void ExtractText(ui::ClipboardBuffer clipboard_buffer,
                   std::optional<ui::DataTransferEndpoint> data_dst,
                   base::OnceCallback<void(std::u16string)> callback);

  // Resets `clipboard_writer_` to write its data to the clipboard, and
  // reinitialize it in preparation for the next write.
  void ResetClipboardWriter();

  // Stops observing clipboard changes and resets the listener.
  void StopObservingClipboard();

  // The owner of this host also owns, and may be, what `context_` points at:
  // the DocumentService for a document, the ServiceWorkerHost for a worker.
  // The destructor must not touch it.
  const std::unique_ptr<Context> context_;

  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;

  // Counts the number of expected `Write*` calls to be made to the current
  // `clipboard_writer_`. This should be used to handle asynchronous `Write*`
  // calls made by `IsClipboardCopyAllowedByPolicy`.
  int pending_writes_ = 0;

  // Indicates that the renderer called `CommitWrite()`, but that
  // `pending_writes_` was not 0 at that time and that it should instead be
  // called when the last pending `Write*` call is made.
  bool pending_commit_write_ = false;

  // Tracks whether this instance is currently observing clipboard changes.
  bool listening_to_clipboard_ = false;

  std::optional<absl::uint128> last_change_id_;

  // Single clipboard listener that will be notified on clipboard changes
  mojo::Remote<blink::mojom::ClipboardListener> clipboard_listener_;

  base::WeakPtrFactory<ClipboardHostImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
