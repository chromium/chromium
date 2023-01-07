// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class MediaInternalsMessageHandler;

// This class is a proxy between MediaInternals (on the IO thread) and
// MediaInternalsMessageHandler (on the UI thread).
// It is ref_counted to ensure that it completes all pending Tasks on both
// threads before destruction.
class MediaInternalsProxy
    : public base::RefCountedThreadSafe<MediaInternalsProxy,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  MediaInternalsProxy();

  MediaInternalsProxy(const MediaInternalsProxy&) = delete;
  MediaInternalsProxy& operator=(const MediaInternalsProxy&) = delete;

  // Register a Handler and start receiving callbacks from MediaInternals.
  void Attach(MediaInternalsMessageHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have MediaInternals send all the data it has.
  void GetEverything();

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<MediaInternalsProxy>;
  virtual ~MediaInternalsProxy();

  void GetEverythingOnIOThread();

  // Callback for MediaInternals to update. Must be called on UI thread.
  static void UpdateUIOnUIThread(MediaInternalsMessageHandler* handler,
                                 const std::u16string& update);

  MediaInternals::UpdateCallback update_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_PROXY_H_
