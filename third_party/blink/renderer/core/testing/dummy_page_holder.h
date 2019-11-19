/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_PAGE_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_PAGE_HOLDER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class IntSize;
class LocalFrame;
class LocalFrameView;
class Settings;

// Creates a dummy Page, LocalFrame, and LocalFrameView whose clients are all
// no-op.
//
// This class can be used when you write unit tests for components which do not
// work correctly without layoutObjects.  To make sure the layoutObjects are
// created, you need to call |frameView().layout()| after you add nodes into
// |document()|.
//
// Since DummyPageHolder stores empty clients in it, it must outlive the Page,
// LocalFrame, LocalFrameView and any other objects created by it.
// DummyPageHolder's destructor ensures this condition by checking remaining
// references to the LocalFrame.

class DummyPageHolder {
  USING_FAST_MALLOC(DummyPageHolder);

 public:
  DummyPageHolder(
      const IntSize& initial_view_size = IntSize(),
      Page::PageClients* = nullptr,
      LocalFrameClient* = nullptr,
      base::OnceCallback<void(Settings&)> setting_overrider =
          base::NullCallback(),
      const base::TickClock* clock = base::DefaultTickClock::GetInstance());
  ~DummyPageHolder();

  Page& GetPage() const;
  LocalFrame& GetFrame() const;
  LocalFrameView& GetFrameView() const;
  Document& GetDocument() const;

 private:
  Persistent<Page> page_;

  // The LocalFrame is accessed from worker threads by unit tests
  // (ThreadableLoaderTest), hence we need to allow cross-thread
  // usage of |m_frame|.
  //
  // TODO: rework the tests to not require cross-thread access.
  CrossThreadPersistent<LocalFrame> frame_;

  Persistent<LocalFrameClient> local_frame_client_;
  DISALLOW_COPY_AND_ASSIGN(DummyPageHolder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DUMMY_PAGE_HOLDER_H_
