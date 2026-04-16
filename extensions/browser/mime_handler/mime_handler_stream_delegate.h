// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_

namespace extensions {

class MimeHandlerStreamDelegate {
 public:
  MimeHandlerStreamDelegate();
  MimeHandlerStreamDelegate(const MimeHandlerStreamDelegate&) = delete;
  MimeHandlerStreamDelegate& operator=(const MimeHandlerStreamDelegate&) =
      delete;
  virtual ~MimeHandlerStreamDelegate();

  virtual bool PluginCanSave() const;
  virtual void SetPluginCanSave(bool plugin_can_save);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_DELEGATE_H_
