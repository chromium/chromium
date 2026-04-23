// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MOCK_MIME_HANDLER_STREAM_DELEGATE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MOCK_MIME_HANDLER_STREAM_DELEGATE_H_

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions::mime_handler {

class MockMimeHandlerStreamDelegate
    : public extensions::MimeHandlerStreamDelegate {
 public:
  MockMimeHandlerStreamDelegate();
  ~MockMimeHandlerStreamDelegate() override;

  MOCK_METHOD(bool, ShouldSetUpPostMessage, (), (const, override));
  MOCK_METHOD(void,
              OnPostMessageSetUp,
              (content::RenderFrameHost*),
              (override));
  MOCK_METHOD(void,
              OnExtensionFrameFinished,
              (content::NavigationHandle*, extensions::StreamInfo*),
              (override));
  MOCK_METHOD(void,
              ValidateContentFrameHost,
              (content::RenderFrameHost*, extensions::StreamInfo*),
              (override));
  MOCK_METHOD(void,
              OnStreamClaimed,
              (content::RenderFrameHost*, extensions::StreamInfo*),
              (override));
  MOCK_METHOD(bool, PluginCanSave, (), (const, override));
  MOCK_METHOD(void, SetPluginCanSave, (bool), (override));
};

}  // namespace extensions::mime_handler

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MOCK_MIME_HANDLER_STREAM_DELEGATE_H_
