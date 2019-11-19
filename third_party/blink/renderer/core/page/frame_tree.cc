/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/page/frame_tree.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/create_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using std::swap;

namespace blink {

namespace {

const unsigned kInvalidChildCount = ~0U;

}  // namespace

FrameTree::FrameTree(Frame* this_frame)
    : this_frame_(this_frame), scoped_child_count_(kInvalidChildCount) {}

FrameTree::~FrameTree() = default;

const AtomicString& FrameTree::GetName() const {
  // TODO(andypaicu): remove this once we have gathered the data
  if (experimental_set_nulled_name_) {
    auto* frame = DynamicTo<LocalFrame>(this_frame_.Get());
    if (!frame)
      frame = DynamicTo<LocalFrame>(&Top());
    if (frame) {
      UseCounter::Count(frame->GetDocument(),
                        WebFeature::kCrossOriginMainFrameNulledNameAccessed);
      if (!name_.IsEmpty()) {
        UseCounter::Count(
            frame->GetDocument(),
            WebFeature::kCrossOriginMainFrameNulledNonEmptyNameAccessed);
      }
    }
  }
  return name_;
}

// TODO(andypaicu): remove this once we have gathered the data
void FrameTree::ExperimentalSetNulledName() {
  experimental_set_nulled_name_ = true;
}

void FrameTree::SetName(const AtomicString& name,
                        ReplicationPolicy replication) {
  if (replication == kReplicate) {
    // Avoid calling out to notify the embedder if the browsing context name
    // didn't change. This is important to avoid violating the browser
    // assumption that the unique name doesn't change if the browsing context
    // name doesn't change.
    // TODO(dcheng): This comment is indicative of a problematic layering
    // violation. The browser should not be relying on the renderer to get this
    // correct; unique name calculation should be moved up into the browser.
    if (name != name_) {
      // TODO(lukasza): https://crbug.com/660485: Eventually we need to also
      // support replication of name changes that originate in a *remote* frame.
      To<LocalFrame>(this_frame_.Get())->Client()->DidChangeName(name);
    }
  }

  // TODO(andypaicu): remove this once we have gathered the data
  experimental_set_nulled_name_ = false;
  name_ = name;
}

DISABLE_CFI_PERF
Frame* FrameTree::Parent() const {
  if (!this_frame_->Client())
    return nullptr;
  return this_frame_->Client()->Parent();
}

Frame& FrameTree::Top() const {
  // FIXME: top() should never return null, so here are some hacks to deal
  // with EmptyLocalFrameClient and cases where the frame is detached
  // already...
  if (!this_frame_->Client())
    return *this_frame_;
  Frame* candidate = this_frame_->Client()->Top();
  return candidate ? *candidate : *this_frame_;
}

Frame* FrameTree::NextSibling() const {
  if (!this_frame_->Client())
    return nullptr;
  return this_frame_->Client()->NextSibling();
}

Frame* FrameTree::FirstChild() const {
  if (!this_frame_->Client())
    return nullptr;
  return this_frame_->Client()->FirstChild();
}

Frame* FrameTree::ScopedChild(unsigned index) const {
  unsigned scoped_index = 0;
  for (Frame* child = FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->Client()->InShadowTree())
      continue;
    if (scoped_index == index)
      return child;
    scoped_index++;
  }

  return nullptr;
}

Frame* FrameTree::ScopedChild(const AtomicString& name) const {
  if (name.IsEmpty())
    return nullptr;

  for (Frame* child = FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->Client()->InShadowTree())
      continue;
    if (child->Tree().GetName() == name)
      return child;
  }
  return nullptr;
}

unsigned FrameTree::ScopedChildCount() const {
  if (scoped_child_count_ == kInvalidChildCount) {
    unsigned scoped_count = 0;
    for (Frame* child = FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (child->Client()->InShadowTree())
        continue;
      scoped_count++;
    }
    scoped_child_count_ = scoped_count;
  }
  return scoped_child_count_;
}

void FrameTree::InvalidateScopedChildCount() {
  scoped_child_count_ = kInvalidChildCount;
}

unsigned FrameTree::ChildCount() const {
  unsigned count = 0;
  for (Frame* result = FirstChild(); result;
       result = result->Tree().NextSibling())
    ++count;
  return count;
}

Frame* FrameTree::FindFrameByName(const AtomicString& name) const {
  // Named frame lookup should always be relative to a local frame.
  DCHECK(IsA<LocalFrame>(this_frame_.Get()));

  Frame* frame = FindFrameForNavigationInternal(name, KURL());
  if (frame && !To<LocalFrame>(this_frame_.Get())->CanNavigate(*frame))
    frame = nullptr;
  return frame;
}

FrameTree::FindResult FrameTree::FindOrCreateFrameForNavigation(
    FrameLoadRequest& request,
    const AtomicString& name) const {
  // Named frame lookup should always be relative to a local frame.
  DCHECK(IsA<LocalFrame>(this_frame_.Get()));
  LocalFrame* current_frame = To<LocalFrame>(this_frame_.Get());

  // A GetNavigationPolicy() value other than kNavigationPolicyCurrentTab at
  // this point indicates that a user event modified the navigation policy
  // (e.g., a ctrl-click). Let the user's action override any target attribute.
  if (request.GetNavigationPolicy() != kNavigationPolicyCurrentTab)
    return FindResult(current_frame, false);

  const KURL& url = request.GetResourceRequest().Url();
  Frame* frame = FindFrameForNavigationInternal(name, url);
  bool new_window = false;
  if (!frame) {
    frame = CreateNewWindow(*current_frame, request, name);
    new_window = true;
    // CreateNewWindow() might have modified NavigationPolicy.
    // Set it back now that the new window is known to be the right one.
    request.SetNavigationPolicy(kNavigationPolicyCurrentTab);
  } else if (!current_frame->CanNavigate(*frame, url)) {
    frame = nullptr;
  }

  if (frame && !new_window) {
    if (frame->GetPage() != current_frame->GetPage())
      frame->GetPage()->GetChromeClient().Focus(current_frame);
    // Focusing can fire onblur, so check for detach.
    if (!frame->GetPage())
      frame = nullptr;
  }
  return FindResult(frame, new_window);
}

Frame* FrameTree::FindFrameForNavigationInternal(const AtomicString& name,
                                                 const KURL& url) const {
  if (EqualIgnoringASCIICase(name, "_current")) {
    UseCounter::Count(
        blink::DynamicTo<blink::LocalFrame>(this_frame_.Get())->GetDocument(),
        WebFeature::kTargetCurrent);
  }

  if (EqualIgnoringASCIICase(name, "_self") ||
      EqualIgnoringASCIICase(name, "_current") || name.IsEmpty())
    return this_frame_;

  if (EqualIgnoringASCIICase(name, "_top"))
    return &Top();

  if (EqualIgnoringASCIICase(name, "_parent"))
    return Parent() ? Parent() : this_frame_.Get();

  // Since "_blank" should never be any frame's name, the following just amounts
  // to an optimization.
  if (EqualIgnoringASCIICase(name, "_blank"))
    return nullptr;

  // Search subtree starting with this frame first.
  for (Frame* frame = this_frame_; frame;
       frame = frame->Tree().TraverseNext(this_frame_)) {
    if (frame->Tree().GetName() == name &&
        To<LocalFrame>(this_frame_.Get())->CanNavigate(*frame, url)) {
      return frame;
    }
  }

  // Search the entire tree for this page next.
  Page* page = this_frame_->GetPage();

  // The frame could have been detached from the page, so check it.
  if (!page)
    return nullptr;

  for (Frame* frame = page->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    // Skip descendants of this frame that were searched above to avoid
    // showing duplicate console messages if a frame is found by name
    // but access is blocked.
    if (frame->Tree().GetName() == name &&
        !frame->Tree().IsDescendantOf(this_frame_.Get()) &&
        To<LocalFrame>(this_frame_.Get())->CanNavigate(*frame, url)) {
      return frame;
    }
  }

  // Search the entire tree of each of the other pages in this namespace.
  for (const Page* other_page : page->RelatedPages()) {
    if (other_page == page || other_page->IsClosing())
      continue;
    for (Frame* frame = other_page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->Tree().GetName() == name &&
          To<LocalFrame>(this_frame_.Get())->CanNavigate(*frame, url)) {
        return frame;
      }
    }
  }

  // Ask the embedder as a fallback.
  LocalFrame* local_frame = To<LocalFrame>(this_frame_.Get());
  Frame* named_frame = local_frame->Client()->FindFrame(name);
  // The embedder can return a frame from another agent cluster. Make sure
  // that the returned frame, if any, has explicitly allowed cross-agent
  // cluster access.
  DCHECK(!named_frame || local_frame->GetDocument()
                             ->GetSecurityOrigin()
                             ->IsGrantedCrossAgentClusterAccess());
  return named_frame;
}

bool FrameTree::IsDescendantOf(const Frame* ancestor) const {
  if (!ancestor)
    return false;

  if (this_frame_->GetPage() != ancestor->GetPage())
    return false;

  for (Frame* frame = this_frame_; frame; frame = frame->Tree().Parent()) {
    if (frame == ancestor)
      return true;
  }
  return false;
}

DISABLE_CFI_PERF
Frame* FrameTree::TraverseNext(const Frame* stay_within) const {
  Frame* child = FirstChild();
  if (child) {
    DCHECK(!stay_within || child->Tree().IsDescendantOf(stay_within));
    return child;
  }

  if (this_frame_ == stay_within)
    return nullptr;

  Frame* sibling = NextSibling();
  if (sibling) {
    DCHECK(!stay_within || sibling->Tree().IsDescendantOf(stay_within));
    return sibling;
  }

  Frame* frame = this_frame_;
  while (!sibling && (!stay_within || frame->Tree().Parent() != stay_within)) {
    frame = frame->Tree().Parent();
    if (!frame)
      return nullptr;
    sibling = frame->Tree().NextSibling();
  }

  if (frame) {
    DCHECK(!stay_within || !sibling ||
           sibling->Tree().IsDescendantOf(stay_within));
    return sibling;
  }

  return nullptr;
}

void FrameTree::Trace(blink::Visitor* visitor) {
  visitor->Trace(this_frame_);
}

}  // namespace blink

#if DCHECK_IS_ON()

static void printIndent(int indent) {
  for (int i = 0; i < indent; ++i)
    printf("    ");
}

static void printFrames(const blink::Frame* frame,
                        const blink::Frame* targetFrame,
                        int indent) {
  if (frame == targetFrame) {
    printf("--> ");
    printIndent(indent - 1);
  } else {
    printIndent(indent);
  }

  auto* local_frame = blink::DynamicTo<blink::LocalFrame>(frame);
  blink::LocalFrameView* view = local_frame ? local_frame->View() : nullptr;
  printf("Frame %p %dx%d\n", frame, view ? view->Width() : 0,
         view ? view->Height() : 0);
  printIndent(indent);
  printf("  owner=%p\n", frame->Owner());
  printIndent(indent);
  printf("  frameView=%p\n", view);
  printIndent(indent);
  printf("  document=%p\n", local_frame ? local_frame->GetDocument() : nullptr);
  printIndent(indent);
  printf("  uri=%s\n\n",
         local_frame
             ? local_frame->GetDocument()->Url().GetString().Utf8().c_str()
             : nullptr);

  for (blink::Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    printFrames(child, targetFrame, indent + 1);
}

void showFrameTree(const blink::Frame* frame) {
  if (!frame) {
    printf("Null input frame\n");
    return;
  }

  printFrames(&frame->Tree().Top(), frame, 0);
}

#endif  // DCHECK_IS_ON()
