// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIPBOARD_WIN_H_
#define REMOTING_HOST_CLIPBOARD_WIN_H_

#include <windows.h>

#include <memory>

#include "base/win/message_window.h"
#include "remoting/host/clipboard.h"

namespace remoting {

namespace protocol {
class ClipboardStub;
}  // namespace protocol

// Interface for Win32 Clipboard APIs to allow mocking in tests.
class Win32Clipboard {
 public:
  virtual ~Win32Clipboard() = default;
  virtual bool Open(HWND hwnd) = 0;
  virtual void Close() = 0;
  virtual bool Empty() = 0;
  virtual bool SetData(UINT format, HGLOBAL data) = 0;
  virtual HGLOBAL GetData(UINT format) = 0;
  virtual bool IsFormatAvailable(UINT format) = 0;
  virtual bool AddFormatListener(HWND hwnd) = 0;
  virtual bool RemoveFormatListener(HWND hwnd) = 0;
};

class ClipboardWin : public Clipboard {
 public:
  explicit ClipboardWin(std::unique_ptr<Win32Clipboard> api);

  ClipboardWin(const ClipboardWin&) = delete;
  ClipboardWin& operator=(const ClipboardWin&) = delete;

  ~ClipboardWin() override;

  // Clipboard interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  friend class ClipboardWinTest;

  void OnClipboardUpdate();

  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result);

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;
  std::unique_ptr<Win32Clipboard> api_;
  std::unique_ptr<base::win::MessageWindow> window_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIPBOARD_WIN_H_
