// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_EI_INPUT_INJECTOR_H_
#define REMOTING_HOST_LINUX_EI_INPUT_INJECTOR_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/host/linux/clipboard_gnome.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class Clipboard;
class EiSenderSession;
class EiKeymap;

class EiInputInjector : public InputInjector {
 public:
  // `stream_manager` is used to retrieve the mapping_id for injecting absolute
  // mouse motion.
  EiInputInjector(base::WeakPtr<EiSenderSession> session,
                  base::WeakPtr<const CaptureStreamManager> stream_manager,
                  std::unique_ptr<Clipboard> clipboard);
  ~EiInputInjector() override;

  base::WeakPtr<EiInputInjector> GetWeakPtr();

  void SetKeymap(base::WeakPtr<EiKeymap> keymap);

  // InputInjector implementation
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

  // InputStub implementation
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // ClipboardStub implementation
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  base::WeakPtr<EiSenderSession> ei_session_;
  base::WeakPtr<EiKeymap> keymap_;
  base::WeakPtr<const CaptureStreamManager> stream_manager_;
  std::unique_ptr<Clipboard> clipboard_;
  std::set<uint32_t> pressed_keys_;

  base::WeakPtrFactory<EiInputInjector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_EI_INPUT_INJECTOR_H_
