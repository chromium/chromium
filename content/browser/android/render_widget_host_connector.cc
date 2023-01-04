// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/render_widget_host_connector.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// Observes RenderWidgetHostViewAndroid to keep the instance up to date.
class RenderWidgetHostConnector::Observer
    : public WebContentsObserver,
      public WebContentsAndroid::DestructionObserver,
      public RenderWidgetHostViewAndroid::DestructionObserver {
 public:
  Observer(WebContents* web_contents, RenderWidgetHostConnector* connector);

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  ~Observer() override;

  // WebContentsObserver implementation.
  void RenderViewReady() override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  // WebContentsAndroid::DestructionObserver implementation.
  void WebContentsAndroidDestroyed(
      WebContentsAndroid* web_contents_android) override;

  // RenderWidgetHostViewAndroid::DestructionObserver implementation.
  void RenderWidgetHostViewDestroyed(
      RenderWidgetHostViewAndroid* rwhva) override;

  void DestroyEarly();
  void UpdateRenderWidgetHostView(RenderWidgetHostViewAndroid* new_rwhva);
  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() const;
  RenderWidgetHostViewAndroid* active_rwhva() const { return active_rwhva_; }

 private:
  void DoDestroy(WebContentsAndroid* web_contents_android);

  const raw_ptr<RenderWidgetHostConnector> connector_;

  // Active RenderWidgetHostView connected to this instance.
  raw_ptr<RenderWidgetHostViewAndroid> active_rwhva_;
};

RenderWidgetHostConnector::Observer::Observer(
    WebContents* web_contents,
    RenderWidgetHostConnector* connector)
    : WebContentsObserver(web_contents),
      connector_(connector),
      active_rwhva_(nullptr) {
  static_cast<WebContentsImpl*>(web_contents)
      ->GetWebContentsAndroid()
      ->AddDestructionObserver(this);
}

RenderWidgetHostConnector::Observer::~Observer() {
  DCHECK(!active_rwhva_);
}

void RenderWidgetHostConnector::Observer::RenderViewReady() {
  UpdateRenderWidgetHostView(GetRenderWidgetHostViewAndroid());
}

void RenderWidgetHostConnector::Observer::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame()) {
    return;
  }

  auto* new_view = new_host ? static_cast<RenderWidgetHostViewBase*>(
                                  new_host->GetRenderWidgetHost()->GetView())
                            : nullptr;
  DCHECK(!new_view || !new_view->IsRenderWidgetHostViewChildFrame());
  auto* new_view_android = static_cast<RenderWidgetHostViewAndroid*>(new_view);
  UpdateRenderWidgetHostView(new_view_android);
}

void RenderWidgetHostConnector::Observer::WebContentsAndroidDestroyed(
    WebContentsAndroid* web_contents_android) {
  DoDestroy(web_contents_android);
}

void RenderWidgetHostConnector::Observer::DestroyEarly() {
  DoDestroy(
      static_cast<WebContentsImpl*>(web_contents())->GetWebContentsAndroid());
}

void RenderWidgetHostConnector::Observer::DoDestroy(
    WebContentsAndroid* web_contents_android) {
  web_contents_android->RemoveDestructionObserver(this);
  DCHECK(!active_rwhva_ || active_rwhva_ == GetRenderWidgetHostViewAndroid());
  UpdateRenderWidgetHostView(nullptr);
  delete connector_;
}

void RenderWidgetHostConnector::Observer::RenderWidgetHostViewDestroyed(
    RenderWidgetHostViewAndroid* destroyed_rwhva) {
  // Null out the raw pointer here and in the connector impl to keep
  // them from referencing the rwvha about to be destroyed.
  if (destroyed_rwhva == active_rwhva_)
    UpdateRenderWidgetHostView(nullptr);
}

void RenderWidgetHostConnector::Observer::UpdateRenderWidgetHostView(
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (active_rwhva_ == new_rwhva)
    return;
  if (active_rwhva_)
    active_rwhva_->RemoveDestructionObserver(this);
  if (new_rwhva)
    new_rwhva->AddDestructionObserver(this);

  connector_->UpdateRenderProcessConnection(active_rwhva_, new_rwhva);
  active_rwhva_ = new_rwhva;
}

RenderWidgetHostViewAndroid*
RenderWidgetHostConnector::Observer::GetRenderWidgetHostViewAndroid() const {
  RenderWidgetHostView* rwhv = web_contents()->GetRenderWidgetHostView();
  DCHECK(!rwhv || !static_cast<RenderWidgetHostViewBase*>(rwhv)
                       ->IsRenderWidgetHostViewChildFrame());
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

RenderWidgetHostConnector::RenderWidgetHostConnector(WebContents* web_contents)
    : render_widget_observer_(new Observer(web_contents, this)) {}

void RenderWidgetHostConnector::Initialize() {
  render_widget_observer_->UpdateRenderWidgetHostView(
      render_widget_observer_->GetRenderWidgetHostViewAndroid());
}

RenderWidgetHostConnector::~RenderWidgetHostConnector() {}

RenderWidgetHostViewAndroid* RenderWidgetHostConnector::GetRWHVAForTesting()
    const {
  return render_widget_observer_->active_rwhva();
}

void RenderWidgetHostConnector::DestroyEarly() {
  render_widget_observer_->DestroyEarly();
}

WebContents* RenderWidgetHostConnector::web_contents() const {
  return render_widget_observer_->web_contents();
}

}  // namespace content
