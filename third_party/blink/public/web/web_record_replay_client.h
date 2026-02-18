// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_RECORD_REPLAY_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_RECORD_REPLAY_CLIENT_H_

namespace blink {

class WebFormControlElement;
class WebInputElement;
class WebNode;

class WebRecordReplayClient {
 public:
  virtual void DidReceiveLeftMouseDownOrGestureTapInNode(const WebNode&) {}
  virtual void SelectControlSelectionChanged(const WebFormControlElement&) {}
  virtual void TextFieldDidEndEditing(const WebInputElement&) {}

 protected:
  virtual ~WebRecordReplayClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_RECORD_REPLAY_CLIENT_H_
