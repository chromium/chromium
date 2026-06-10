// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard_win.h"

#include <windows.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_hglobal.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

namespace {

// Hardening: limit clipboard text to 1MB to prevent DoS.
const size_t kMaxClipboardSize = 1024 * 1024;

class Win32ClipboardImpl : public Win32Clipboard {
 public:
  bool Open(HWND hwnd) override {
    const int kMaxAttemptsToOpenClipboard = 5;
    const base::TimeDelta kSleepTimeBetweenAttempts = base::Milliseconds(5);

    // This code runs on the UI thread, so we can block only very briefly.
    for (int attempt = 0; attempt < kMaxAttemptsToOpenClipboard; ++attempt) {
      if (attempt > 0) {
        base::PlatformThread::Sleep(kSleepTimeBetweenAttempts);
      }
      if (::OpenClipboard(hwnd)) {
        return true;
      }
    }
    return false;
  }

  void Close() override {
    // CloseClipboard() must be called with anonymous access token. See
    // crbug.com/441834 .
    BOOL result = ::ImpersonateAnonymousToken(::GetCurrentThread());
    CHECK(result);
    ::CloseClipboard();
    result = ::RevertToSelf();
    CHECK(result);
  }

  bool Empty() override { return ::EmptyClipboard(); }

  bool SetData(UINT format, HGLOBAL data) override {
    return ::SetClipboardData(format, data) != NULL;
  }

  HGLOBAL GetData(UINT format) override { return ::GetClipboardData(format); }

  bool IsFormatAvailable(UINT format) override {
    return ::IsClipboardFormatAvailable(format);
  }

  bool AddFormatListener(HWND hwnd) override {
    return ::AddClipboardFormatListener(hwnd);
  }

  bool RemoveFormatListener(HWND hwnd) override {
    return ::RemoveClipboardFormatListener(hwnd);
  }
};

class ScopedClipboard {
 public:
  explicit ScopedClipboard(Win32Clipboard* api) : api_(api), opened_(false) {}

  ScopedClipboard(const ScopedClipboard&) = delete;
  ScopedClipboard& operator=(const ScopedClipboard&) = delete;

  ~ScopedClipboard() {
    if (opened_) {
      api_->Close();
    }
  }

  bool Init(HWND hwnd) {
    DCHECK(!opened_);
    if (api_->Open(hwnd)) {
      opened_ = true;
    }
    return opened_;
  }

  bool Empty() {
    DCHECK(opened_);
    return api_->Empty();
  }

  bool SetData(UINT format, HGLOBAL data) {
    DCHECK(opened_);
    return api_->SetData(format, data);
  }

  HGLOBAL GetData(UINT format) {
    DCHECK(opened_);
    return api_->GetData(format);
  }

 private:
  raw_ptr<Win32Clipboard> api_;
  bool opened_;
};

struct GlobalFreeTraits {
  using Handle = HGLOBAL;
  static bool CloseHandle(HGLOBAL handle) {
    return ::GlobalFree(handle) == NULL;
  }
  static bool IsHandleValid(HGLOBAL handle) { return handle != NULL; }
  static HGLOBAL NullHandle() { return NULL; }
};

using ScopedGlobalAlloc =
    base::win::GenericScopedHandle<GlobalFreeTraits,
                                   base::win::DummyVerifierTraits>;

}  // namespace

ClipboardWin::ClipboardWin(std::unique_ptr<Win32Clipboard> api)
    : api_(std::move(api)) {}

ClipboardWin::~ClipboardWin() {
  if (window_) {
    api_->RemoveFormatListener(window_->hwnd());
  }
}

void ClipboardWin::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  DCHECK(!window_);

  client_clipboard_ = std::move(client_clipboard);

  window_ = std::make_unique<base::win::MessageWindow>();
  if (!window_->Create(base::BindRepeating(&ClipboardWin::HandleMessage,
                                           base::Unretained(this)))) {
    LOG(ERROR) << "Couldn't create clipboard window.";
    window_.reset();
    return;
  }

  if (!api_->AddFormatListener(window_->hwnd())) {
    LOG(WARNING) << "AddClipboardFormatListener() failed: " << GetLastError();
  }
}

void ClipboardWin::InjectClipboardEvent(const protocol::ClipboardEvent& event) {
  if (!window_) {
    return;
  }

  if (event.mime_type() != kMimeTypeTextUtf8) {
    return;
  }

  // Hardening: limit to 1MB to prevent DoS.
  if (event.data().size() > kMaxClipboardSize) {
    LOG(WARNING) << "Clipboard payload too large: " << event.data().size()
                 << " bytes. Dropping event.";
    return;
  }

  if (!base::IsStringUTF8AllowingNoncharacters(event.data())) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded.";
    return;
  }

  std::u16string text = base::UTF8ToUTF16(ReplaceLfByCrLf(event.data()));

  const size_t num_chars = text.size() + 1;
  ScopedGlobalAlloc text_global(
      ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, num_chars * sizeof(WCHAR)));
  if (!text_global.is_valid()) {
    LOG(WARNING) << "Couldn't allocate global memory.";
    return;
  }

  {
    base::win::ScopedHGlobal<LPWSTR> locked(text_global.get());
    if (!locked.data()) {
      LOG(WARNING) << "Couldn't lock global memory.";
      return;
    }

    // SAFETY: Have to trust GlobalAlloc/GlobalLock returned num_chars WCHARs.
    auto dest_span = UNSAFE_BUFFERS(base::span(locked.data(), num_chars));

    // Use as_writable_chars to view both sides as compatible character types.
    // This bypasses the wchar_t vs char16_t strict aliasing check.
    auto dest_chars = base::as_writable_chars(dest_span);
    auto src_chars = base::as_chars(base::span(text));

    dest_chars.first(src_chars.size()).copy_from(src_chars);

    // Set the null terminator using the original span (which is WCHAR).
    dest_span[text.size()] = L'\0';
  }

  ScopedClipboard clipboard(api_.get());
  if (!clipboard.Init(window_->hwnd())) {
    LOG(WARNING) << "Couldn't open the clipboard.";
    return;
  }

  if (!clipboard.Empty()) {
    LOG(WARNING) << "Couldn't empty the clipboard.";
    return;
  }

  if (!clipboard.SetData(CF_UNICODETEXT, text_global.get())) {
    LOG(WARNING) << "Couldn't set clipboard data.";
    return;
  }

  // The system now owns the memory.
  (void)text_global.release();
}

void ClipboardWin::OnClipboardUpdate() {
  DCHECK(window_);

  if (!client_clipboard_) {
    return;
  }

  if (api_->IsFormatAvailable(CF_UNICODETEXT)) {
    std::string utf8_text;
    // Add a scope, so that we keep the clipboard open for as short a time as
    // possible.
    {
      ScopedClipboard clipboard(api_.get());
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
      if (!text_lock.data()) {
        LOG(WARNING) << "Couldn't lock clipboard data: " << GetLastError();
        return;
      }

      size_t characters = text_lock.size() / sizeof(WCHAR);
      if (characters > kMaxClipboardSize) {
        LOG(WARNING) << "Clipboard data too large: " << characters
                     << " characters. Dropping update.";
        return;
      }

      // Safe read: Use the known size of the HGLOBAL allocation to bound the
      // string assignment, rather than relying on null termination.
      std::wstring_view text(text_lock.data(), characters);
      size_t null_pos = text.find(L'\0');
      if (null_pos != std::wstring_view::npos) {
        text = text.substr(0, null_pos);
      }

      utf8_text = base::WideToUTF8(text);
    }

    if (utf8_text.size() > kMaxClipboardSize) {
      LOG(WARNING) << "UTF-8 clipboard data too large: " << utf8_text.size()
                   << " bytes. Dropping update.";
      return;
    }

    protocol::ClipboardEvent event;
    event.set_mime_type(kMimeTypeTextUtf8);
    event.set_data(ReplaceCrLfByLf(utf8_text));

    client_clipboard_->InjectClipboardEvent(event);
  }
}

bool ClipboardWin::HandleMessage(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 LRESULT* result) {
  if (message == WM_CLIPBOARDUPDATE) {
    OnClipboardUpdate();
    *result = 0;
    return true;
  }

  return false;
}

std::unique_ptr<Clipboard> Clipboard::Create() {
  return std::make_unique<ClipboardWin>(std::make_unique<Win32ClipboardImpl>());
}

}  // namespace remoting
