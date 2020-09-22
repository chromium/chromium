// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RAW_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RAW_CLIPBOARD_HOST_IMPL_H_

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {
class ScopedClipboardWriter;
}  // namespace ui

namespace content {

class RenderFrameHost;

// Instances destroy themselves when the blink::mojom::RawClipboardHost is
// disconnected, and this can only be used on the frame and sequence it's
// created on.
class CONTENT_EXPORT RawClipboardHostImpl
    : public blink::mojom::RawClipboardHost {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver);
  RawClipboardHostImpl(const RawClipboardHostImpl&) = delete;
  RawClipboardHostImpl& operator=(const RawClipboardHostImpl&) = delete;
  ~RawClipboardHostImpl() override;

 private:
  RawClipboardHostImpl(
      mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver,
      RenderFrameHost* render_frame_host);

  // mojom::RawClipboardHost.
  void ReadAvailableFormatNames(
      ReadAvailableFormatNamesCallback callback) override;
  void Read(const base::string16& format, ReadCallback callback) override;
  void Write(const base::string16& format, mojo_base::BigBuffer data) override;
  void CommitWrite() override;

  std::unique_ptr<ui::ClipboardDataEndpoint> CreateDataEndpoint();
  bool HasTransientUserActivation() const;

  // The render frame is not owned.
  const GlobalFrameRoutingId render_frame_routing_id_;

  mojo::Receiver<blink::mojom::RawClipboardHost> receiver_;
  ui::Clipboard* const clipboard_;  // Not owned.
  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RAW_CLIPBOARD_HOST_IMPL_H_