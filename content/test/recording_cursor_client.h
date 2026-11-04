// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RECORDING_CURSOR_CLIENT_H_
#define CONTENT_TEST_RECORDING_CURSOR_CLIENT_H_

#include "ui/aura/test/test_cursor_client.h"
#include "ui/gfx/native_ui_types.h"

namespace content {

// A cursor client for tests that remembers the last cursor set on it, as
// aura::test::TestCursorClient only counts calls. Installs itself on the root
// window given to the constructor for its lifetime.
class RecordingCursorClient : public aura::test::TestCursorClient {
 public:
  using TestCursorClient::TestCursorClient;
  RecordingCursorClient(const RecordingCursorClient&) = delete;
  RecordingCursorClient& operator=(const RecordingCursorClient&) = delete;
  ~RecordingCursorClient() override;

  // aura::test::TestCursorClient:
  void SetCursor(gfx::NativeCursor cursor) override;
  gfx::NativeCursor GetCursor() const override;

 private:
  gfx::NativeCursor last_cursor_;
};

}  // namespace content

#endif  // CONTENT_TEST_RECORDING_CURSOR_CLIENT_H_
