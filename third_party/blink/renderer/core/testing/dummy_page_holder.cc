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

#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

class DummyLocalFrameClient : public EmptyLocalFrameClient {
 public:
  DummyLocalFrameClient() = default;

 private:
  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    return Platform::Current()->CreateDefaultURLLoaderFactory();
  }
};

}  // namespace

DummyPageHolder::DummyPageHolder(
    const IntSize& initial_view_size,
    Page::PageClients* page_clients_argument,
    LocalFrameClient* local_frame_client,
    base::OnceCallback<void(Settings&)> setting_overrider,
    const base::TickClock* clock) {
  Page::PageClients page_clients;
  if (!page_clients_argument)
    FillWithEmptyClients(page_clients);
  else
    page_clients.chrome_client = page_clients_argument->chrome_client;
  page_ = Page::CreateNonOrdinary(page_clients);
  Settings& settings = page_->GetSettings();
  if (setting_overrider)
    std::move(setting_overrider).Run(settings);

  local_frame_client_ = local_frame_client;
  if (!local_frame_client_)
    local_frame_client_ = MakeGarbageCollected<DummyLocalFrameClient>();

  // Create new WindowAgentFactory as this page will be isolated from others.
  frame_ =
      MakeGarbageCollected<LocalFrame>(local_frame_client_.Get(), *page_,
                                       /* FrameOwner* */ nullptr,
                                       /* WindowAgentFactory* */ nullptr,
                                       /* InterfaceRegistry* */ nullptr, clock);
  frame_->SetView(
      MakeGarbageCollected<LocalFrameView>(*frame_, initial_view_size));
  frame_->View()->GetPage()->GetVisualViewport().SetSize(initial_view_size);
  frame_->Init();

  CoreInitializer::GetInstance().ProvideModulesToPage(GetPage(), nullptr);
}

DummyPageHolder::~DummyPageHolder() {
  page_->WillBeDestroyed();
  page_.Clear();
  frame_.Clear();
}

Page& DummyPageHolder::GetPage() const {
  return *page_;
}

LocalFrame& DummyPageHolder::GetFrame() const {
  DCHECK(frame_);
  return *frame_;
}

LocalFrameView& DummyPageHolder::GetFrameView() const {
  return *frame_->View();
}

Document& DummyPageHolder::GetDocument() const {
  return *frame_->DomWindow()->document();
}

}  // namespace blink
