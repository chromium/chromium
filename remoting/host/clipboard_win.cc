// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include <windows.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/win/message_window.h"
#include "base/win/scoped_hglobal.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace {

// A scoper class that opens and closes the clipboard.
// This class was adapted from the ScopedClipboard class in
// ui/base/clipboard/clipboard_win.cc.
class ScopedClipboard {
 public:
  ScopedClipboard() : opened_(false) {
  }

  ~ScopedClipboard() {
    if (opened_) {
      // CloseClipboard() must be called with anonymous access token. See
      // crbug.com/441834 .
      BOOL result = ::ImpersonateAnonymousToken(::GetCurrentThread());
      CHECK(result);
      ::CloseClipboard();
      result = ::RevertToSelf();
      CHECK(result);
    }
  }

  bool Init(HWND owner) {
    const int kMaxAttemptsToOpenClipboard = 5;
    const base::TimeDelta kSleepTimeBetweenAttempts =
        base::TimeDelta::FromMilliseconds(5);

    if (opened_) {
      NOTREACHED();
      return true;
    }

    // This code runs on the UI thread, so we can block only very briefly.
    for (int attempt = 0; attempt < kMaxAttemptsToOpenClipboard; ++attempt) {
      if (attempt > 0) {
        base::PlatformThread::Sleep(kSleepTimeBetweenAttempts);
      }
      if (::OpenClipboard(owner)) {
        opened_ = true;
        return true;
      }
    }
    return false;
  }

  BOOL Empty() {
    if (!opened_) {
      NOTREACHED();
      return false;
    }
    return ::EmptyClipboard();
  }

  void SetData(UINT uFormat, HANDLE hMem) {
    if (!opened_) {
      NOTREACHED();
      return;
    }
    // The caller must not close the handle that ::SetClipboardData returns.
    ::SetClipboardData(uFormat, hMem);
  }

  // The caller must not free the handle. The caller should lock the handle,
  // copy the clipboard data, and unlock the handle. All this must be done
  // before this ScopedClipboard is destroyed.
  HANDLE GetData(UINT format) {
    if (!opened_) {
      NOTREACHED();
      return nullptr;
    }
    return ::GetClipboardData(format);
  }

 private:
  bool opened_;
};

}  // namespace

namespace remoting {

class ClipboardWin : public Clipboard {
 public:
  ClipboardWin();
  ~ClipboardWin() override;

  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) override;

 private:
  void OnClipboardUpdate();

  // Handles messages received by |window_|.
  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;
  // Used to subscribe to WM_CLIPBOARDUPDATE messages.
  std::unique_ptr<base::win::MessageWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardWin);
};

ClipboardWin::ClipboardWin() {}

ClipboardWin::~ClipboardWin() {
  if (window_)
    ::RemoveClipboardFormatListener(window_->hwnd());
}

void ClipboardWin::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(!window_);

  client_clipboard_.swap(client_clipboard);

  window_.reset(new base::win::MessageWindow());
  if (!window_->Create(base::Bind(&ClipboardWin::HandleMessage,
                                  base::Unretained(this)))) {
    LOG(ERROR) << "Couldn't create clipboard window.";
    window_.reset();
    return;
  }

  if (!::AddClipboardFormatListener(window_->hwnd())) {
    LOG(WARNING) << "AddClipboardFormatListener() failed: " << GetLastError();
  }
}

void ClipboardWin::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  if (!window_)
    return;

  // Currently we only handle UTF-8 text.
  if (event.mime_type().compare(kMimeTypeTextUtf8) != 0)
    return;
  if (!StringIsUtf8(event.data().c_str(), event.data().length())) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded.";
    return;
  }

  base::string16 text = base::UTF8ToUTF16(ReplaceLfByCrLf(event.data()));

  ScopedClipboard clipboard;
  if (!clipboard.Init(window_->hwnd())) {
    LOG(WARNING) << "Couldn't open the clipboard.";
    return;
  }

  clipboard.Empty();

  HGLOBAL text_global =
      ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(WCHAR));
  if (!text_global) {
    LOG(WARNING) << "Couldn't allocate global memory.";
    return;
  }

  LPWSTR text_global_locked =
      reinterpret_cast<LPWSTR>(::GlobalLock(text_global));
  memcpy(text_global_locked, text.data(), text.size() * sizeof(WCHAR));
  text_global_locked[text.size()] = (WCHAR)0;
  ::GlobalUnlock(text_global);

  clipboard.SetData(CF_UNICODETEXT, text_global);
}

void ClipboardWin::OnClipboardUpdate() {
  DCHECK(window_);

  if (::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    base::string16 text;
    // Add a scope, so that we keep the clipboard open for as short a time as
    // possible.
    {
      ScopedClipboard clipboard;
      if (!clipboard.Init(window_->hwnd())) {
        LOG(WARNING) << "Couldn't open the clipboard." << GetLastError();
        return;
      }

      HGLOBAL text_global = clipboard.GetData(CF_UNICODETEXT);
      if (!text_global) {
        LOG(WARNING) << "Couldn't get data from the clipboard: "
                     << GetLastError();
        return;
      }

      base::win::ScopedHGlobal<WCHAR*> text_lock(text_global);
      if (!text_lock.get()) {
        LOG(WARNING) << "Couldn't lock clipboard data: " << GetLastError();
        return;
      }
      text.assign(text_lock.get());
    }

    protocol::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(ReplaceCrLfByLf(base::UTF16ToUTF8(text)));

    if (client_clipboard_.get()) {
      client_clipboard_->InjectClipboardEvent(event);
    }
  }
}

bool ClipboardWin::HandleMessage(
    UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  if (message == WM_CLIPBOARDUPDATE) {
    OnClipboardUpdate();
    *result = 0;
    return true;
  }

  return false;
}

std::unique_ptr<Clipboard> Clipboard::Create() {
  return base::WrapUnique(new ClipboardWin());
}

}  // namespace remoting
