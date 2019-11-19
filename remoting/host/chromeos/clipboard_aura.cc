// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/clipboard_aura.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace {

// Clipboard polling interval in milliseconds.
const int64_t kClipboardPollingIntervalMs = 500;

}  // namespace

namespace remoting {

ClipboardAura::ClipboardAura()
    : current_change_count_(0),
      polling_interval_(
          base::TimeDelta::FromMilliseconds(kClipboardPollingIntervalMs)) {
}

ClipboardAura::~ClipboardAura() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ClipboardAura::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(thread_checker_.CalledOnValidThread());

  client_clipboard_ = std::move(client_clipboard);

  // Aura doesn't provide a clipboard-changed notification. The only way to
  // detect clipboard changes is by polling.
  clipboard_polling_timer_.Start(FROM_HERE, polling_interval_, this,
                                 &ClipboardAura::CheckClipboardForChanges);
}

void ClipboardAura::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0) {
    return;
  }

  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(base::UTF8ToUTF16(event.data()));

  // Update local change-count to prevent this change from being picked up by
  // CheckClipboardForChanges.
  current_change_count_++;
}

void ClipboardAura::SetPollingIntervalForTesting(
    base::TimeDelta polling_interval) {
  DCHECK(thread_checker_.CalledOnValidThread());

  polling_interval_ = polling_interval;
}

void ClipboardAura::CheckClipboardForChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  uint64_t change_count =
      clipboard->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  if (change_count == current_change_count_) {
    return;
  }

  current_change_count_ = change_count;

  protocol::ClipboardEvent event;
  std::string data;

  clipboard->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste, &data);
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data(data);

  client_clipboard_->InjectClipboardEvent(event);
}

std::unique_ptr<Clipboard> Clipboard::Create() {
  return base::WrapUnique(new ClipboardAura());
}

}  // namespace remoting
