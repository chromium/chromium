// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_FONT_CACHE_DISPATCHER_WIN_H_
#define CONTENT_PUBLIC_COMMON_FONT_CACHE_DISPATCHER_WIN_H_

#include <windows.h>

#include "content/common/content_export.h"
#include "content/public/common/font_cache_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// Dispatches messages used for font caching on Windows. This is needed because
// Windows can't load fonts into its kernel cache in sandboxed processes. So the
// sandboxed process asks the browser process to do this for it.
class CONTENT_EXPORT FontCacheDispatcher : public mojom::FontCacheWin {
 public:
  FontCacheDispatcher();

  FontCacheDispatcher(const FontCacheDispatcher&) = delete;
  FontCacheDispatcher& operator=(const FontCacheDispatcher&) = delete;

  ~FontCacheDispatcher() override;

  static void Create(mojo::PendingReceiver<mojom::FontCacheWin> receiver);

 private:
  // mojom::FontCacheWin:
  void PreCacheFont(const LOGFONT& log_font,
                    PreCacheFontCallback callback) override;
  void ReleaseCachedFonts() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_FONT_CACHE_DISPATCHER_WIN_H_
