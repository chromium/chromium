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

  // Represents the underlying type of the argument passed to
  // IsClipboardPasteAllowedCallback without the const& part.
  using IsClipboardPasteAllowedCallbackArgType =
      std::optional<ClipboardPasteData>;

  // Keeps track of a request to see if some clipboard content, identified by
  // its sequence number, is allowed to be pasted into the RenderFrameHost
  // that owns this clipboard host.
  //
  // A request starts in the state incomplete until Complete() is called with
  // a value.  Callbacks can be added to the request before or after it has
  // completed.
  class CONTENT_EXPORT IsPasteAllowedRequest {
   public:
    IsPasteAllowedRequest();
    ~IsPasteAllowedRequest();

    // Adds |callback| to be notified when the request completes.  If the
    // request is already completed |callback| is invoked immediately.  Returns
    // true if a request should be started after adding this callback.
    bool AddCallback(IsClipboardPasteAllowedCallback callback);

    // Mark this request as completed with the specified result.
    // Invoke all callbacks now.
    void Complete(IsClipboardPasteAllowedCallbackArgType data);

    // Returns true if the request has completed.
    bool is_complete() const { return data_.has_value(); }

    // Returns true if this request is obsolete.  An obsolete request
    // is one that is completed, all registered callbacks have been
    // called, and is considered old.
    //
    // |now| represents the current time.  It is an argument to ease testing.
    bool IsObsolete(base::Time now);

    // Returns the time at which this request was completed.  If called
    // before the request is completed the return value is undefined.
    base::Time completed_time();

   private:
    // Calls all the callbacks in |callbacks_| with the current value of
    // |allowed_|.  |allowed_| must not be empty.
    void InvokeCallbacks();

    // The time at which the request was completed.  Before completion this
    // value is undefined.
    base::Time completed_time_;

    // The data argument to pass to the IsClipboardPasteAllowedCallback.
    // This member is null until Complete() is called.
    std::optional<IsClipboardPasteAllowedCallbackArgType> data_;
    std::vector<IsClipboardPasteAllowedCallback> callbacks_;
  };

  // A paste allowed request is obsolete if it is older than this time.
  static const base::TimeDelta kIsPasteAllowedRequestTooOld;

  explicit ClipboardHostImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  // Performs a check to see if pasting `data` is allowed by data transfer
  // policies and invokes FinishPasteIfAllowed upon completion.
  void PasteIfPolicyAllowed(ui::ClipboardBuffer clipboard_buffer,
                            const ui::ClipboardFormatType& data_type,
                            ClipboardPasteData clipboard_paste_data,
                            IsClipboardPasteAllowedCallback callback);

  // Remove obsolete entries from the outstanding requests map.
  // A request is obsolete if:
  //  - its sequence number is less than |seqno|
  //  - it has no callbacks
  //  - it is too old
  void CleanupObsoleteRequests();

  // Completion callback of PerformPasteIfAllowed(). Sets the allowed
  // status for the clipboard data corresponding to sequence number |seqno|.
  void FinishPasteIfAllowed(
      const ui::ClipboardSequenceNumberToken& seqno,
      std::optional<ClipboardPasteData> clipboard_paste_data);

  const std::map<ui::ClipboardSequenceNumberToken, IsPasteAllowedRequest>&
  is_paste_allowed_requests_for_testing() {
    return is_allowed_requests_;
  }

  // Called by PerformPasteIfAllowed() when an is allowed request is
  // needed. Virtual to be overridden in tests.
  virtual void StartIsPasteAllowedRequest(
      const ui::ClipboardSequenceNumberToken& seqno,
      const ui::ClipboardFormatType& data_type,
      ui::ClipboardBuffer clipboard_buffer,
      ClipboardPasteData clipboard_paste_data);

 private:
  friend class ClipboardHostImplTest;
  friend class ClipboardHostImplScanTest;
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplTest,
                           IsPasteAllowedRequest_AddCallback);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplTest,
                           IsPasteAllowedRequest_Complete);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplTest,
                           IsPasteAllowedRequest_IsObsolete);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplScanTest,
                           PerformPasteIfAllowed_EmptyData);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplScanTest, PerformPasteIfAllowed);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplScanTest,
                           PerformPasteIfAllowed_SameHost_NotStarted);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplScanTest,
                           PerformPasteIfAllowed_External_Started);
  FRIEND_TEST_ALL_PREFIXES(ClipboardHostImplScanTest, GetSourceEndpoint);

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
  void ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                      const std::u16string& type,
                      ReadCustomDataCallback callback) override;
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
  void WriteCustomData(
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

  // Helpers to be used when checking if data is allowed to be copied.
  // If `replacement_data` is null, `clipboard_writer_` will be used to write
  // the corresponding text/markup data to the clipboard. If it is not, instead
  // write the replacement string to the clipboard as plaintext. This can be
  // called asynchronously.
  void OnCopyTextAllowedResult(const std::u16string& text,
                               std::optional<std::u16string> replacement_data);
  void OnCopyHtmlAllowedResult(const GURL& url,
                               const std::u16string& markup,
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

  // Outstanding is allowed requests per clipboard contents.  Maps a clipboard
  // sequence number to an outstanding request.
  std::map<ui::ClipboardSequenceNumberToken, IsPasteAllowedRequest>
      is_allowed_requests_;

  base::WeakPtrFactory<ClipboardHostImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
