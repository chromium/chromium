// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_

#include <stdint.h>

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/widget_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Instantiated per RenderProcessHost to provide various optimizations on
// behalf of a RenderWidgetHost.  This class bridges between the IO thread
// where the RenderProcessHost's MessageFilter lives and the UI thread where
// the RenderWidgetHost lives.
class RenderWidgetHelper
    : public base::RefCountedThreadSafe<RenderWidgetHelper,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  RenderWidgetHelper();

  RenderWidgetHelper(const RenderWidgetHelper&) = delete;
  RenderWidgetHelper& operator=(const RenderWidgetHelper&) = delete;

  void Init(int render_process_id);

  // Gets the next available routing id.  This is thread safe.
  int GetNextRoutingID();

  // Retrieve a previously stored data. Returns true if the tokens
  // were found.
  bool TakeStoredDataForFrameToken(const blink::LocalFrameToken& frame_token,
                                   int32_t& routing_id,
                                   base::UnguessableToken& devtools_frame_token,
                                   blink::DocumentToken& document_token);

  // Store a set of frame tokens given a routing id. This is usually called on
  // the IO thread, and |GetFrameTokensForFrameRoutingID| will be called on the
  // UI thread at a later point.
  void StoreNextFrameRoutingID(
      int32_t routing_id,
      const blink::LocalFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token,
      const blink::DocumentToken& document_token);

  // IO THREAD ONLY -----------------------------------------------------------

 private:
  friend class base::RefCountedThreadSafe<RenderWidgetHelper>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<RenderWidgetHelper>;

  ~RenderWidgetHelper();

  int render_process_id_;

  struct FrameTokens {
    FrameTokens(int32_t routing_id,
                const base::UnguessableToken& devtools_frame_token,
                const blink::DocumentToken& document_token);
    FrameTokens(const FrameTokens& other);
    FrameTokens& operator=(const FrameTokens& other);
    ~FrameTokens();

    int32_t routing_id;
    base::UnguessableToken devtools_frame_token;
    blink::DocumentToken document_token;
  };

  // Lock that is used to provide access to `frame_storage_map_`
  // from the IO and UI threads.
  base::Lock frame_token_map_lock_;

  // Map that stores handed out routing IDs and frame tokens. Items
  // will be removed from this table in TakeStoredDataForFrameToken.
  // Locked by |lock_|
  std::map<blink::LocalFrameToken, FrameTokens> frame_storage_map_
      GUARDED_BY(frame_token_map_lock_);

  // The next routing id to use.
  base::AtomicSequenceNumber next_routing_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HELPER_H_
