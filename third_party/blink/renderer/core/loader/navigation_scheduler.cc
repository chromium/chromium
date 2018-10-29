/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"

#include <memory>
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader_state_machine.h"
#include "third_party/blink/renderer/core/loader/scheduled_navigation.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

unsigned NavigationDisablerForBeforeUnload::navigation_disable_count_ = 0;

class ScheduledURLNavigation : public ScheduledNavigation {
 protected:
  ScheduledURLNavigation(Reason reason,
                         double delay,
                         Document* origin_document,
                         const KURL& url,
                         WebFrameLoadType frame_load_type,
                         bool is_location_change,
                         base::TimeTicks input_timestamp)
      : ScheduledNavigation(reason,
                            delay,
                            origin_document,
                            is_location_change,
                            input_timestamp),
        url_(url),
        should_check_main_world_content_security_policy_(
            kCheckContentSecurityPolicy),
        frame_load_type_(frame_load_type) {
    if (ContentSecurityPolicy::ShouldBypassMainWorld(origin_document)) {
      should_check_main_world_content_security_policy_ =
          kDoNotCheckContentSecurityPolicy;
    }

    if (origin_document && url.ProtocolIs("blob") &&
        BlobUtils::MojoBlobURLsEnabled()) {
      origin_document->GetPublicURLManager().Resolve(
          url_, MakeRequest(&blob_url_token_));
    }
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest request(OriginDocument(), ResourceRequest(url_), "_self",
                             should_check_main_world_content_security_policy_);
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
    request.SetInputStartTime(InputTimestamp());

    if (blob_url_token_) {
      mojom::blink::BlobURLTokenPtr token_clone;
      blob_url_token_->Clone(MakeRequest(&token_clone));
      request.SetBlobURLToken(std::move(token_clone));
    }

    frame->Loader().StartNavigation(request, frame_load_type_);
  }

  KURL Url() const override { return url_; }

  WebFrameLoadType LoadType() const { return frame_load_type_; }

 private:
  KURL url_;
  mojom::blink::BlobURLTokenPtr blob_url_token_;
  ContentSecurityPolicyDisposition
      should_check_main_world_content_security_policy_;
  WebFrameLoadType frame_load_type_;
};

class ScheduledRedirect final : public ScheduledURLNavigation {
 public:
  static ScheduledRedirect* Create(double delay,
                                   Document* origin_document,
                                   const KURL& url,
                                   Document::HttpRefreshType http_refresh_type,
                                   WebFrameLoadType frame_load_type,
                                   base::TimeTicks input_timestamp) {
    return new ScheduledRedirect(delay, origin_document, url, http_refresh_type,
                                 frame_load_type, input_timestamp);
  }

  bool ShouldStartTimer(LocalFrame* frame) override {
    return frame->GetDocument()->LoadEventFinished();
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest request(OriginDocument(), ResourceRequest(Url()), "_self");
    request.SetInputStartTime(InputTimestamp());
    WebFrameLoadType load_type = LoadType();
    if (EqualIgnoringFragmentIdentifier(frame->GetDocument()->Url(),
                                        request.GetResourceRequest().Url())) {
      request.GetResourceRequest().SetCacheMode(
          mojom::FetchCacheMode::kValidateCache);
      load_type = WebFrameLoadType::kReload;
    }
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
    frame->Loader().StartNavigation(request, load_type);
  }

 private:
  static Reason ToReason(Document::HttpRefreshType http_refresh_type) {
    switch (http_refresh_type) {
      case Document::HttpRefreshType::kHttpRefreshFromHeader:
        return Reason::kHttpHeaderRefresh;
      case Document::HttpRefreshType::kHttpRefreshFromMetaTag:
        return Reason::kMetaTagRefresh;
      default:
        break;
    }
    NOTREACHED();
    return Reason::kMetaTagRefresh;
  }

  ScheduledRedirect(double delay,
                    Document* origin_document,
                    const KURL& url,
                    Document::HttpRefreshType http_refresh_type,
                    WebFrameLoadType frame_load_type,
                    base::TimeTicks input_timestamp)
      : ScheduledURLNavigation(ToReason(http_refresh_type),
                               delay,
                               origin_document,
                               url,
                               frame_load_type,
                               false,
                               input_timestamp) {
    ClearUserGesture();
  }
};

class ScheduledFrameNavigation final : public ScheduledURLNavigation {
 public:
  static ScheduledFrameNavigation* Create(Document* origin_document,
                                          const KURL& url,
                                          WebFrameLoadType frame_load_type,
                                          base::TimeTicks input_timestamp) {
    return new ScheduledFrameNavigation(origin_document, url, frame_load_type,
                                        input_timestamp);
  }

 private:
  ScheduledFrameNavigation(Document* origin_document,
                           const KURL& url,
                           WebFrameLoadType frame_load_type,
                           base::TimeTicks input_timestamp)
      : ScheduledURLNavigation(Reason::kFrameNavigation,
                               0.0,
                               origin_document,
                               url,
                               frame_load_type,
                               !url.ProtocolIsJavaScript(),
                               input_timestamp) {}
};

class ScheduledReload final : public ScheduledNavigation {
 public:
  static ScheduledReload* Create(LocalFrame* frame,
                                 base::TimeTicks input_timestamp) {
    return new ScheduledReload(frame, input_timestamp);
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    ResourceRequest resource_request = frame->Loader().ResourceRequestForReload(
        WebFrameLoadType::kReload, ClientRedirectPolicy::kClientRedirect);
    if (resource_request.IsNull())
      return;
    FrameLoadRequest request = FrameLoadRequest(nullptr, resource_request);
    request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
    request.SetInputStartTime(InputTimestamp());
    frame->Loader().StartNavigation(request, WebFrameLoadType::kReload);
  }

  KURL Url() const override { return frame_->GetDocument()->Url(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(frame_);
    ScheduledNavigation::Trace(visitor);
  }

 private:
  explicit ScheduledReload(LocalFrame* frame, base::TimeTicks input_timestamp)
      : ScheduledNavigation(Reason::kReload,
                            0.0,
                            nullptr /*origin_document */,
                            true,
                            input_timestamp),
        frame_(frame) {
    DCHECK(frame->GetDocument());
  }

  Member<LocalFrame> frame_;
};

class ScheduledPageBlock final : public ScheduledNavigation {
 public:
  static ScheduledPageBlock* Create(Document* origin_document, int reason) {
    return new ScheduledPageBlock(origin_document, reason);
  }

  void Fire(LocalFrame* frame) override {
    frame->Client()->LoadErrorPage(reason_);
  }

  KURL Url() const override { return KURL(); }

 private:
  ScheduledPageBlock(Document* origin_document, int reason)
      : ScheduledNavigation(Reason::kPageBlock,
                            0.0,
                            origin_document,
                            true,
                            base::TimeTicks() /* input_timestamp */),
        reason_(reason) {}

  int reason_;
};

class ScheduledFormSubmission final : public ScheduledNavigation {
 public:
  static ScheduledFormSubmission* Create(Document* document,
                                         FormSubmission* submission,
                                         WebFrameLoadType frame_load_type,
                                         base::TimeTicks input_timestamp) {
    return new ScheduledFormSubmission(document, submission, frame_load_type,
                                       input_timestamp);
  }

  void Fire(LocalFrame* frame) override {
    std::unique_ptr<UserGestureIndicator> gesture_indicator =
        CreateUserGestureIndicator();
    FrameLoadRequest frame_request =
        submission_->CreateFrameLoadRequest(OriginDocument());
    frame_request.SetInputStartTime(InputTimestamp());
    frame->Loader().StartNavigation(frame_request, frame_load_type_,
                                    submission_->GetNavigationPolicy());
  }

  KURL Url() const override { return submission_->RequestURL(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(submission_);
    ScheduledNavigation::Trace(visitor);
  }

 private:
  ScheduledFormSubmission(Document* document,
                          FormSubmission* submission,
                          WebFrameLoadType frame_load_type,
                          base::TimeTicks input_timestamp)
      : ScheduledNavigation(submission->Method() == FormSubmission::kGetMethod
                                ? Reason::kFormSubmissionGet
                                : Reason::kFormSubmissionPost,
                            0,
                            document,
                            true,
                            input_timestamp),
        submission_(submission),
        frame_load_type_(frame_load_type) {
    DCHECK_NE(submission->Method(), FormSubmission::kDialogMethod);
    DCHECK(submission_->Form());
  }

  Member<FormSubmission> submission_;
  WebFrameLoadType frame_load_type_;
};

NavigationScheduler::NavigationScheduler(LocalFrame* frame) : frame_(frame) {}

NavigationScheduler::~NavigationScheduler() {
}

bool NavigationScheduler::LocationChangePending() {
  return redirect_ && redirect_->IsLocationChange();
}

bool NavigationScheduler::IsNavigationScheduledWithin(double interval) const {
  return redirect_ && redirect_->Delay() <= interval;
}

// TODO(dcheng): There are really two different load blocking concepts at work
// here and they have been incorrectly tangled together.
//
// 1. NavigationDisablerForBeforeUnload is for blocking navigation scheduling
//    during a beforeunload events. Scheduled navigations during beforeunload
//    would make it possible to get trapped in an endless loop of beforeunload
//    dialogs.
//
//    Checking Frame::isNavigationAllowed() doesn't make sense in this context:
//    NavigationScheduler is always cleared when a new load commits, so it's
//    impossible for a scheduled navigation to clobber a navigation that just
//    committed.
//
// 2. FrameNavigationDisabler / LocalFrame::isNavigationAllowed() are intended
//    to prevent Documents from being reattached during destruction, since it
//    can cause bugs with security origin confusion. This is primarily intended
//    to block /synchronous/ navigations during things lke
//    Document::detachLayoutTree().
inline bool NavigationScheduler::ShouldScheduleReload() const {
  return frame_->GetPage() && frame_->IsNavigationAllowed() &&
         NavigationDisablerForBeforeUnload::IsNavigationAllowed();
}

inline bool NavigationScheduler::ShouldScheduleNavigation(
    const KURL& url) const {
  return frame_->GetPage() && frame_->IsNavigationAllowed() &&
         (url.ProtocolIsJavaScript() ||
          NavigationDisablerForBeforeUnload::IsNavigationAllowed());
}

void NavigationScheduler::ScheduleRedirect(
    double delay,
    const KURL& url,
    Document::HttpRefreshType http_refresh_type) {
  if (!ShouldScheduleNavigation(url))
    return;
  if (delay < 0 || delay > INT_MAX / 1000)
    return;
  if (url.IsEmpty())
    return;

  // We want a new back/forward list item if the refresh timeout is > 1 second.
  if (!redirect_ || delay <= redirect_->Delay()) {
    WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
    if (delay <= 1)
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
    Schedule(ScheduledRedirect::Create(delay, frame_->GetDocument(), url,
                                       http_refresh_type, frame_load_type,
                                       InputTimestamp()));
  }
}

bool NavigationScheduler::MustReplaceCurrentItem(LocalFrame* target_frame) {
  // Non-user navigation before the page has finished firing onload should not
  // create a new back/forward item. See https://webkit.org/b/42861 for the
  // original motivation for this.
  if (!target_frame->GetDocument()->LoadEventFinished() &&
      !LocalFrame::HasTransientUserActivation(target_frame))
    return true;

  // Navigation of a subframe during loading of an ancestor frame does not
  // create a new back/forward item. The definition of "during load" is any time
  // before all handlers for the load event have been run. See
  // https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation
  // for this.
  Frame* parent_frame = target_frame->Tree().Parent();
  return parent_frame && parent_frame->IsLocalFrame() &&
         !ToLocalFrame(parent_frame)->Loader().AllAncestorsAreComplete();
}

base::TimeTicks NavigationScheduler::InputTimestamp() {
  if (const WebInputEvent* input_event = CurrentInputEvent::Get()) {
    return input_event->TimeStamp();
  }
  return base::TimeTicks();
}

void NavigationScheduler::ScheduleFrameNavigation(
    Document* origin_document,
    const KURL& url,
    WebFrameLoadType frame_load_type) {
  if (!ShouldScheduleNavigation(url))
    return;

  if (MustReplaceCurrentItem(frame_))
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;

  base::TimeTicks input_timestamp = InputTimestamp();
  // If the URL we're going to navigate to is the same as the current one,
  // except for the fragment part, we don't need to schedule the location
  // change. We'll skip this optimization for cross-origin navigations to
  // minimize the navigator's ability to execute timing attacks.
  if (origin_document->GetSecurityOrigin()->CanAccess(
          frame_->GetDocument()->GetSecurityOrigin())) {
    if (url.HasFragmentIdentifier() &&
        EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url)) {
      FrameLoadRequest request(origin_document, ResourceRequest(url), "_self");
      request.SetInputStartTime(input_timestamp);
      if (frame_load_type == WebFrameLoadType::kReplaceCurrentItem)
        request.SetClientRedirect(ClientRedirectPolicy::kClientRedirect);
      frame_->Loader().StartNavigation(request, frame_load_type);
      return;
    }
  }

  Schedule(ScheduledFrameNavigation::Create(origin_document, url,
                                            frame_load_type, input_timestamp));
}

void NavigationScheduler::SchedulePageBlock(Document* origin_document,
                                            int reason) {
  DCHECK(frame_->GetPage());
  Schedule(ScheduledPageBlock::Create(origin_document, reason));
}

void NavigationScheduler::ScheduleFormSubmission(Document* document,
                                                 FormSubmission* submission) {
  DCHECK(frame_->GetPage());
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
  if (MustReplaceCurrentItem(frame_))
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  Schedule(ScheduledFormSubmission::Create(document, submission,
                                           frame_load_type, InputTimestamp()));
}

void NavigationScheduler::ScheduleReload() {
  if (!ShouldScheduleReload())
    return;
  if (frame_->GetDocument()->Url().IsEmpty())
    return;
  Schedule(ScheduledReload::Create(frame_, InputTimestamp()));
}

void NavigationScheduler::NavigateTask() {
  if (!frame_->GetPage())
    return;
  if (frame_->GetPage()->Paused()) {
    probe::frameClearedScheduledNavigation(frame_);
    return;
  }

  ScheduledNavigation* redirect(redirect_.Release());
  redirect->Fire(frame_);
  probe::frameClearedScheduledNavigation(frame_);
}

void NavigationScheduler::Schedule(ScheduledNavigation* redirect) {
  DCHECK(frame_->GetPage());

  // In a back/forward navigation, we sometimes restore history state to
  // iframes, even though the state was generated dynamically and JS will try to
  // put something different in the iframe. In this case, we will load stale
  // things and/or confuse the JS when it shortly thereafter tries to schedule a
  // location change. Let the JS have its way.
  // FIXME: This check seems out of place.
  if (!frame_->Loader().StateMachine()->CommittedFirstRealDocumentLoad() &&
      frame_->Loader().GetProvisionalDocumentLoader() &&
      frame_->Loader().GetProvisionalDocumentLoader()->DidStart()) {
    frame_->Loader().StopAllLoaders();
    if (!frame_->GetPage())
      return;
  }

  Cancel();
  redirect_ = redirect;
  if (redirect_->IsLocationChange())
    frame_->GetDocument()->SuppressLoadEvent();
  StartTimer();
}

void NavigationScheduler::StartTimer() {
  if (!redirect_)
    return;

  DCHECK(frame_->GetPage());
  if (navigate_task_handle_.IsActive())
    return;
  if (!redirect_->ShouldStartTimer(frame_))
    return;

  // wrapWeakPersistent(this) is safe because a posted task is canceled when the
  // task handle is destroyed on the dtor of this NavigationScheduler.
  navigate_task_handle_ = PostDelayedCancellableTask(
      *frame_->GetFrameScheduler()->GetTaskRunner(TaskType::kInternalLoading),
      FROM_HERE,
      WTF::Bind(&NavigationScheduler::NavigateTask, WrapWeakPersistent(this)),
      TimeDelta::FromSecondsD(redirect_->Delay()));

  probe::frameScheduledNavigation(frame_, redirect_.Get());
}

void NavigationScheduler::Cancel() {
  if (navigate_task_handle_.IsActive()) {
    probe::frameClearedScheduledNavigation(frame_);
  }
  navigate_task_handle_.Cancel();
  redirect_.Clear();
}

void NavigationScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(redirect_);
}

}  // namespace blink
