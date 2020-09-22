// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/raw_clipboard_host_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace content {

void RawClipboardHostImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver) {
  DCHECK(render_frame_host);

  // Feature flags and permission should already be checked in the renderer
  // process, but recheck in the browser process in case of a hijacked renderer.
  if (!base::FeatureList::IsEnabled(blink::features::kRawClipboard)) {
    mojo::ReportBadMessage("Raw Clipboard is not enabled.");
    return;
  }

  // Renderer process should already check for user activation before sending
  // this request. Double check in case of compromised renderer.
  if (!render_frame_host->HasTransientUserActivation()) {
    // mojo::ReportBadMessage() is not appropriate here, because user
    // activation may expire after the renderer check but before the browser
    // check.
    return;
  }

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());

  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForFrame(
          PermissionType::CLIPBOARD_READ_WRITE, render_frame_host,
          render_frame_host->GetLastCommittedOrigin().GetURL());

  if (status != blink::mojom::PermissionStatus::GRANTED) {
    // mojo::ReportBadMessage() is not appropriate here because the permission
    // may be granted after the renderer check, but revoked before the browser
    // check.
    return;
  }

  // Clipboard implementations do interesting things, like run nested message
  // loops. Use manual memory management instead of SelfOwnedReceiver<T> which
  // synchronously destroys on failure and can result in some unfortunate
  // use-after-frees after the nested message loops exit.
  auto* host = new RawClipboardHostImpl(std::move(receiver), render_frame_host);
  host->receiver_.set_disconnect_handler(base::BindOnce(
      [](RawClipboardHostImpl* host) {
        base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, host);
      },
      host));
}

RawClipboardHostImpl::~RawClipboardHostImpl() {
  clipboard_writer_->Reset();
}

RawClipboardHostImpl::RawClipboardHostImpl(
    mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver,
    RenderFrameHost* render_frame_host)
    : render_frame_routing_id_(
          GlobalFrameRoutingId(render_frame_host->GetProcess()->GetID(),
                               render_frame_host->GetRoutingID())),
      receiver_(this, std::move(receiver)),
      clipboard_(ui::Clipboard::GetForCurrentThread()),
      clipboard_writer_(
          new ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste,
                                        CreateDataEndpoint())) {
  DCHECK(render_frame_host);
}

void RawClipboardHostImpl::ReadAvailableFormatNames(
    ReadAvailableFormatNamesCallback callback) {
  if (!HasTransientUserActivation())
    return;
  std::vector<base::string16> raw_types =
      clipboard_->ReadAvailablePlatformSpecificFormatNames(
          ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint().get());
  std::move(callback).Run(raw_types);
}

void RawClipboardHostImpl::Read(const base::string16& format,
                                ReadCallback callback) {
  if (!HasTransientUserActivation())
    return;
  if (format.size() >= kMaxFormatSize) {
    receiver_.ReportBadMessage("Requested format string length too long.");
    return;
  }

  std::string result;
  clipboard_->ReadData(
      ui::ClipboardFormatType::GetType(base::UTF16ToUTF8(format)),
      CreateDataEndpoint().get(), &result);
  base::span<const uint8_t> span(
      reinterpret_cast<const uint8_t*>(result.data()), result.size());
  mojo_base::BigBuffer buffer = mojo_base::BigBuffer(span);
  std::move(callback).Run(std::move(buffer));
}

void RawClipboardHostImpl::Write(const base::string16& format,
                                 mojo_base::BigBuffer data) {
  if (!HasTransientUserActivation())
    return;
  if (format.size() >= kMaxFormatSize) {
    receiver_.ReportBadMessage("Target format string length too long.");
    return;
  }
  if (data.size() >= kMaxDataSize) {
    receiver_.ReportBadMessage("Write data too large.");
    return;
  }

  // Windows / X11 clipboards enter an unrecoverable state after registering
  // some amount of unique formats, and there's no way to un-register these
  // formats. For these clipboards, use a conservative limit to avoid
  // registering too many formats, as:
  // (1) Other native applications may also register clipboard formats.
  // (2) |registered_formats| only persists over one Chrome Clipboard session.
  // (3) Chrome also registers other clipboard formats.
  //
  // The limit is based on Windows, which has the smallest limit, at 0x4000.
  // Windows represents clipboard formats using values in 0xC000 - 0xFFFF.
  // Therefore, Windows supports at most 0x4000 registered formats. Reference:
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclipboardformata
  static constexpr int kMaxWindowsClipboardFormats = 0x4000;
  static constexpr int kMaxRegisteredFormats = kMaxWindowsClipboardFormats / 4;
  static base::NoDestructor<std::set<base::string16>> registered_formats;
  if (!base::Contains(*registered_formats, format)) {
    if (registered_formats->size() >= kMaxRegisteredFormats)
      return;
    registered_formats->emplace(format);
  }

  clipboard_writer_->WriteData(format, std::move(data));
}

void RawClipboardHostImpl::CommitWrite() {
  clipboard_writer_ = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste, CreateDataEndpoint());
}

std::unique_ptr<ui::ClipboardDataEndpoint>
RawClipboardHostImpl::CreateDataEndpoint() {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  if (!render_frame_host)
    return nullptr;

  return std::make_unique<ui::ClipboardDataEndpoint>(
      render_frame_host->GetLastCommittedOrigin());
}

bool RawClipboardHostImpl::HasTransientUserActivation() const {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_routing_id_);
  if (!render_frame_host)
    return false;

  // Renderer process should already check for user activation before sending
  // this request. Double check in case of compromised renderer.
  // mojo::ReportBadMessage() is not appropriate here, because user activation
  // may expire after the renderer check but before the browser check.
  return render_frame_host->HasTransientUserActivation();
}

}  // namespace content
